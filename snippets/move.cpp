struct Test
{
    MyObj mo;
    // const MyObj & getMyObj() const & { return mo; }   // c

    // can only be called by rvalue ref
    MyObj && getMyObj() &&{ return std::move(mo); }           // d
};

MyObj aa = Test().getMyObj();   // e
Test t;
// MyObj bb = t.getMyObj();    // f
