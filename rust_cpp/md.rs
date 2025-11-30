use std::{
    collections::BTreeMap,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

use futures_util::StreamExt;
use serde::Deserialize;
use tokio::{signal, time::sleep};
use tokio_tungstenite::{connect_async, tungstenite::Message};

#[derive(Default, Debug, Clone)]
struct OrderBook {
    bids: BTreeMap<f64, f64>, // descending iteration via rev()
    asks: BTreeMap<f64, f64>, // ascending
}

fn apply_deltas(side: &mut BTreeMap<f64, f64>, deltas: &[(f64, f64)], reverse: bool) {
    for &(p, sz) in deltas {
        if sz == 0.0 {
            side.remove(&p);
        } else {
            side.insert(p, sz);
        }
    }
    if reverse {
        // keep as-is; we’ll iterate with .iter().rev()
    }
}

#[derive(Deserialize)]
struct Snapshot {
    lastUpdateId: i64,
    bids: Vec<[String; 2]>,
    asks: Vec<[String; 2]>,
}

#[derive(Deserialize)]
struct WsUpdate {
    U: i64,
    u: i64,
    #[serde(default)]
    pu: i64,
    b: Vec<[String; 2]>,
    a: Vec<[String; 2]>,
}

fn now_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as i64
}

fn parse_side(raw: &[[String; 2]]) -> Vec<(f64, f64)> {
    raw.iter()
        .map(|arr| (arr[0].parse().unwrap_or(0.0), arr[1].parse().unwrap_or(0.0)))
        .collect()
}

fn print_book(book: &OrderBook, depth: usize, proc_us: i64, recv_ts_ms: i64) {
    print!("[BINANCE] ts_ms={} proc_us={} top {} levels\n  Bids: ", recv_ts_ms, proc_us, depth);
    let mut i = 0;
    for (p, s) in book.bids.iter().rev() {
        if i >= depth { break; }
        print!("{:.6}@{:.6}  ", p, s);
        i += 1;
    }
    if i == 0 { print!("(empty)"); }
    print!("\n  Asks: ");
    i = 0;
    for (p, s) in book.asks.iter() {
        if i >= depth { break; }
        print!("{:.6}@{:.6}  ", p, s);
        i += 1;
    }
    if i == 0 { print!("(empty)"); }
    print!("\n\n");
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let symbol = std::env::args().nth(1).unwrap_or_else(|| "BTCUSDT".to_string());
    let ws_symbol = symbol.to_lowercase();

    let mut term = signal::ctrl_c();
    println!("Starting Binance depth stream for {} (Ctrl+C to quit)", symbol);

    while term.try_recv().is_err() {
        if let Err(e) = run_once(&symbol, &ws_symbol).await {
            eprintln!("[BINANCE] error: {e} — retrying...");
            sleep(Duration::from_secs(1)).await;
        }
    }
    println!("Stopped");
    Ok(())
}

async fn run_once(symbol: &str, ws_symbol: &str) -> anyhow::Result<()> {
    // 1) Connect WS first
    let url = format!("wss://stream.binance.com:9443/ws/{}@depth@100ms", ws_symbol);
    let (ws_stream, _) = connect_async(&url).await?;
    let (mut ws_write, mut ws_read) = ws_stream.split();

    // 2) REST snapshot
    let snap_url = format!("https://api.binance.com/api/v3/depth?symbol={}&limit=1000", symbol);
    let snap: Snapshot = reqwest::get(&snap_url).await?.json().await?;
    let mut book = OrderBook::default();
    for (p, s) in parse_side(&snap.bids) {
        if s > 0.0 { book.bids.insert(p, s); }
    }
    for (p, s) in parse_side(&snap.asks) {
        if s > 0.0 { book.asks.insert(p, s); }
    }
    let mut last_id = snap.lastUpdateId;

    // 3) Bridge buffered updates until we get one that advances last_id
    loop {
        let msg = ws_read.next().await.ok_or_else(|| anyhow::anyhow!("WS closed"))??;
        let t_recv = Instant::now();
        let recv_ms = now_ms();
        let update = match msg {
            Message::Text(txt) => serde_json::from_str::<WsUpdate>(&txt)?,
            Message::Binary(bin) => serde_json::from_slice::<WsUpdate>(&bin)?,
            _ => continue,
        };
        apply_update(&update, &mut last_id, &mut book)?;
        print_book(&book, 10, t_recv.elapsed().as_micros() as i64, recv_ms);
        break;
    }

    // 4) Steady-state
    while let Some(msg) = ws_read.next().await {
        let msg = msg?;
        let t_recv = Instant::now();
        let recv_ms = now_ms();
        match msg {
            Message::Text(txt) => {
                let update: WsUpdate = serde_json::from_str(&txt)?;
                apply_update(&update, &mut last_id, &mut book)?;
                print_book(&book, 10, t_recv.elapsed().as_micros() as i64, recv_ms);
            }
            Message::Binary(bin) => {
                let update: WsUpdate = serde_json::from_slice(&bin)?;
                apply_update(&update, &mut last_id, &mut book)?;
                print_book(&book, 10, t_recv.elapsed().as_micros() as i64, recv_ms);
            }
            Message::Ping(p) => ws_write.send(Message::Pong(p)).await?,
            Message::Close(_) => anyhow::bail!("WS closed"),
            _ => {}
        }
    }
    Ok(())
}

fn apply_update(update: &WsUpdate, last_id: &mut i64, book: &mut OrderBook) -> anyhow::Result<()> {
    let U = update.U;
    let u = update.u;
    let pu = update.pu;

    if u <= *last_id {
        return Ok(());
    }
    let bridged_window = U <= *last_id + 1 && *last_id + 1 <= u;
    let bridged_pu = pu != 0 && pu == *last_id;
    if !(bridged_window || bridged_pu) {
        anyhow::bail!("sequence gap; need resnapshot");
    }

    let bid_d = parse_side(&update.b);
    let ask_d = parse_side(&update.a);
    apply_deltas(&mut book.bids, &bid_d, true);
    apply_deltas(&mut book.asks, &ask_d, false);
    *last_id = u;
    Ok(())
}

