# Numeric And CAS Dependencies

This section is the practical answer to the precision requirement.

## Numeric

- `GMP`
  arbitrary-precision integers and rationals
- `MPFR`
  arbitrary-precision floating point with correct rounding
- `FLINT`
  fast finite-field and polynomial arithmetic, plus ball arithmetic in modern releases
- `Boost.Multiprecision`
  C++ interface options including `mpfr_float`
- `rug`
  main Rust-side arbitrary-precision binding layer

## CAS / symbolic layer

- `Fermat`
  historical simplifier used by Kira/FIRE
- `GiNaC` and `CLN`
  symbolic-expression layer used by Kira
