use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{anyhow, bail, Result};
use futures_util::{SinkExt, StreamExt};
use ordered_float::NotNan;
use serde::Deserialize;
use std::collections::BTreeMap;
use tokio::{signal, time::sleep};
use tokio_tungstenite::{connect_async, tungstenite::Message};

const UPDATE_LIMIT: usize = 100;

#[derive(Default, Debug, Clone)]
struct OrderBook {
    bids: BTreeMap<NotNan<f64>, f64>, // iterate with .iter().rev() for descending
    asks: BTreeMap<NotNan<f64>, f64>, // ascending by default
}

fn apply_deltas(side: &mut BTreeMap<NotNan<f64>, f64>, deltas: &[(NotNan<f64>, f64)]) {
    for &(p, sz) in deltas {
        if sz == 0.0 {
            side.remove(&p);
        } else {
            side.insert(p, sz);
        }
    }
}

#[derive(Deserialize)]
struct Snapshot {
    #[serde(rename = "lastUpdateId")]
    last_update_id: i64,
    bids: Vec<[String; 2]>,
    asks: Vec<[String; 2]>,
}

#[derive(Deserialize)]
struct WsUpdate {
    #[serde(rename = "U")]
    u_start: i64,
    #[serde(rename = "u")]
    u_end: i64,
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
        .map(|arr| {
            let p = arr[0].parse().unwrap_or(0.0);
            let s = arr[1].parse().unwrap_or(0.0);
            (p, s)
        })
        .collect()
}

fn print_book(book: &OrderBook, depth: usize, proc_us: i64, recv_ts_ms: i64) {
    print!(
        "[BINANCE] ts_ms={} proc_us={} top {} levels\n  Bids: ",
        recv_ts_ms, proc_us, depth
    );
    let mut i = 0;
    for (p, s) in book.bids.iter().rev() {
        if i >= depth {
            break;
        }
        print!("{:.6}@{:.6}  ", p.into_inner(), s);
        i += 1;
    }
    if i == 0 {
        print!("(empty)");
    }

    print!("\n  Asks: ");
    i = 0;
    for (p, s) in book.asks.iter() {
        if i >= depth {
            break;
        }
        print!("{:.6}@{:.6}  ", p.into_inner(), s);
        i += 1;
    }
    if i == 0 {
        print!("(empty)");
    }
    print!("\n\n");
}

fn percentile(sorted: &[i64], pct: f64) -> i64 {
    if sorted.is_empty() {
        return 0;
    }
    let pos = ((pct / 100.0) * (sorted.len().saturating_sub(1) as f64)).round() as usize;
    sorted[pos.min(sorted.len() - 1)]
}

fn print_latency_stats(latencies: &[i64]) {
    if latencies.is_empty() {
        println!("[STATS] no samples collected");
        return;
    }
    let mut v = latencies.to_vec();
    v.sort_unstable();
    let min = v.first().copied().unwrap_or(0);
    let max = v.last().copied().unwrap_or(0);
    let p10 = percentile(&v, 10.0);
    let p50 = percentile(&v, 50.0);
    let p90 = percentile(&v, 90.0);
    let p99 = percentile(&v, 99.0);
    println!(
        "[STATS] samples={} min={}us max={}us p10={}us p50={}us p90={}us p99={}us",
        v.len(),
        min,
        max,
        p10,
        p50,
        p90,
        p99
    );
}

#[tokio::main]
async fn main() -> Result<()> {
    let symbol = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "BTCUSDT".to_string());
    let ws_symbol = symbol.to_lowercase();

    println!(
        "Starting Binance depth stream for {} (Ctrl+C to quit)",
        symbol
    );

    loop {
        tokio::select! {
            _ = signal::ctrl_c() => {
                println!("Stopped");
                return Ok(());
            }
            res = run_once(&symbol, &ws_symbol) => {
                match res {
                    Ok(()) => return Ok(()), // completed after hitting limit
                    Err(e) => {
                        eprintln!("[BINANCE] error: {e} — retrying...");
                        sleep(Duration::from_secs(1)).await;
                    }
                }
            }
        }
    }
}

async fn run_once(symbol: &str, ws_symbol: &str) -> Result<()> {
    let mut latencies_us: Vec<i64> = Vec::with_capacity(UPDATE_LIMIT);

    // 1) Connect WS first
    let url = format!("wss://stream.binance.com:9443/ws/{}@depth@100ms", ws_symbol);
    let (ws_stream, _) = connect_async(&url).await?;
    let (mut ws_write, mut ws_read) = ws_stream.split();

    // 2) REST snapshot
    let snap_url = format!(
        "https://api.binance.com/api/v3/depth?symbol={}&limit=1000",
        symbol
    );
    let snap: Snapshot = reqwest::get(&snap_url).await?.json().await?;

    let mut book = OrderBook::default();
    for (p, s) in parse_side(&snap.bids) {
        if s > 0.0 {
            if let Ok(np) = NotNan::new(p) {
                book.bids.insert(np, s);
            }
        }
    }
    for (p, s) in parse_side(&snap.asks) {
        if s > 0.0 {
            if let Ok(np) = NotNan::new(p) {
                book.asks.insert(np, s);
            }
        }
    }
    let mut last_id = snap.last_update_id;

    // 3) Bridge buffered updates (stop once we accept the first advancing update)
    loop {
        let msg = ws_read
            .next()
            .await
            .ok_or_else(|| anyhow!("WS closed during bridge"))??;
        let t_recv = Instant::now();
        let recv_ms = now_ms();

        let update = match msg {
            Message::Text(txt) => serde_json::from_str::<WsUpdate>(&txt)?,
            Message::Binary(bin) => serde_json::from_slice::<WsUpdate>(&bin)?,
            _ => continue,
        };

        apply_update(&update, &mut last_id, &mut book)?;
        let proc = t_recv.elapsed().as_micros() as i64;
        latencies_us.push(proc);
        print_book(&book, 10, proc, recv_ms);
        if latencies_us.len() >= UPDATE_LIMIT {
            print_latency_stats(&latencies_us);
            return Ok(());
        }
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
                let proc = t_recv.elapsed().as_micros() as i64;
                latencies_us.push(proc);
                print_book(&book, 10, proc, recv_ms);
            }
            Message::Binary(bin) => {
                let update: WsUpdate = serde_json::from_slice::<WsUpdate>(&bin)?;
                apply_update(&update, &mut last_id, &mut book)?;
                let proc = t_recv.elapsed().as_micros() as i64;
                latencies_us.push(proc);
                print_book(&book, 10, proc, recv_ms);
            }
            Message::Ping(p) => ws_write.send(Message::Pong(p)).await?,
            Message::Close(_) => bail!("WS closed"),
            _ => {}
        }

        if latencies_us.len() >= UPDATE_LIMIT {
            print_latency_stats(&latencies_us);
            return Ok(());
        }
    }

    Ok(())
}

fn apply_update(update: &WsUpdate, last_id: &mut i64, book: &mut OrderBook) -> Result<()> {
    let u_start = update.u_start;
    let u_end = update.u_end;
    let pu = update.pu;

    if u_end <= *last_id {
        return Ok(());
    }

    let bridged_window = u_start <= *last_id + 1 && *last_id + 1 <= u_end;
    let bridged_pu = pu != 0 && pu == *last_id;
    if !(bridged_window || bridged_pu) {
        bail!("sequence gap; need resnapshot");
    }

    let bid_d = parse_side(&update.b)
        .into_iter()
        .filter_map(|(p, s)| NotNan::new(p).ok().map(|np| (np, s)))
        .collect::<Vec<_>>();
    let ask_d = parse_side(&update.a)
        .into_iter()
        .filter_map(|(p, s)| NotNan::new(p).ok().map(|np| (np, s)))
        .collect::<Vec<_>>();
    apply_deltas(&mut book.bids, &bid_d);
    apply_deltas(&mut book.asks, &ask_d);
    *last_id = u_end;
    Ok(())
}
