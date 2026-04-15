acq400ioc implements counters using two parts:
1. A generic "counters.db" that provides smoothing, logic and all the PV's 
		(eg ACTIVE, COUNT, FREQ)
2. A specific "scan the register" record in another file 

There are actually 2 generic chains available, "counter.db" .. used the most often, and counter.db ,  used for WR counters.   It appears that the the WR counters agressively tried to avoid a glitchy "ACTIVE" state, and most likely this is too aggressive.

Worked example, PPS:

specific: wr.db
```tcl
record(longin, "${UUT}:${SITE}:WR:PPS:rc") {
    field(SCAN, "1 second")
    field(DTYP, "stream")
    field(INP, "@wr.proto getCount(wr_pps_client_count) ${SPORT}")
    field(DESC, "PPS Count")
    field(PRIO, "LOW")
}
```

generic: counter.db
```tcl
# first, comp integer rollover; then calc active now; now report active if now or previous
# ie hold ACTIVE for at lease one cycle: stretch a single pulse, avoid PPS blinks

record(calc, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE") {
    field(INPA, "${UUT}:${SITE}:${SIG}:${lname}:rc")
    field(INPB, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE.LA")
    field(INPC, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE")
    field(INPD, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE.LC")
    field(INPE, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE.LD")
    field(INPF, "${UUT}:${SITE}:${SIG}:${lname}:ACTIVE.LE")
    field(INPL, "4294967296")
    field(CALC, "C||D||E||F; A:=A<0?L+A:A; C:=(A-B)?1:0")
    field(PREC, "1")
    field(FLNK, "${UUT}:${SITE}:${SIG}:${lname}:nzcount")
    field(DESC, "True is signal changed from last")
}
```

A COMMENT!  HOKOYO!   Can't claim to understand this completely now, but the postive feedback of INPC might have something to do with it.

For reference, the standard logic is:

```tcl
record(calc, "${UUT}:${SITE}:SIG:${lname}:ACTIVE") {
	field(INPA, "${UUT}:${SITE}:SIG:${lname}:rawcount")
	field(INPB, "${UUT}:${SITE}:SIG:${lname}:ACTIVE.LA")
	field(INPL, "${CTRMAX}")
	field(CALC, "A:=A<B?L+A:A; (A-B) ? 1: 0")
	field(PREC, "1")
	field(FLNK, "${UUT}:${SITE}:SIG:${lname}:nzcount")
	field(DESC, "True is signal changed from last")
}
```

In the first instance, revert to the standard logic and see what it's like.

It's very quick to patch the IOC db: make changes either in a db file or in /tmp/st.cmd, then press ^X in epics-console, the IOC restarts and you can see the effect of your change right away!
