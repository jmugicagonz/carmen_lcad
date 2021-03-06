VAR
  long PinA, PinB, PinAtH_Addr, PinBtH_Addr  'tHa, tHb
  long cog, stack[20] ' For object

CON
  _clkmode = xtal1 + pll16x ' System clock → 80 MHz
  _xinfreq = 5_000_000

PUB Start(_PinA, _PinB, _PinAtH_Addr, _PinBtH_Addr) | us
  PinA := _PinA
  PinB := _PinB
  
  PinAtH_Addr := _PinAtH_Addr
  PinBtH_Addr := _PinBtH_Addr

  cognew(RunLoop, @stack)

PUB RunLoop | us, t, tC

  us := clkfreq/1_000_000
  
  ctra[30..26] := ctrb[30..26] := %00100                ' Counters A and B → NCO single-ended 
  ctra[30..26] := ctrb[30..26] := %00100                ' Counters A and B → NCO single-ended
  ctra[5..0] := PinA                                    ' Set pins for counters to control
  ctrb[5..0] := PinB
  frqa := frqb := 1                                     ' Add 1 to phs with each clock tick
  dira[PinA] := dira[PinB] := 1                         ' Set I/O pins to output
  tC := 64 * us -50                                        ' Set up cycle time                                 
    
  t := cnt                                              ' Mark current time.
  
  repeat

    phsa := long[PinAtH_Addr]'-(( 64 * long[PinAtH_Addr] / 100 ) * us)
    phsb := long[PinBtH_Addr]'-(( 64 * long[PinBtH_Addr] / 100 ) * us)

    t += tC                                             ' Calculate next cycle repeat
    waitcnt(t)                                          ' Wait for next cycle