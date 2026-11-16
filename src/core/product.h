// The product's own name, for the handful of shared strings that say it.
//
// One codebase ships under two names. The Android and desktop app is
// Retro-3DO and is published under that name; the iOS app is Spryte, because
// the Retro-3DO App Store record was rejected under guideline 4.3 and a bundle
// identifier cannot be changed on an existing record, so iOS ships as a
// distinct product (see docs/PROVENANCE.md for where the name comes from --
// "spryte" is the 3DO's own word for the primitive its graphics hardware
// draws).
//
// The walkthrough in guide.cpp is deliberately written once for every
// platform, so the name inside those sentences cannot be a literal. It is a
// macro rather than a function so the strings stay compile-time constants and
// the guide can keep handing out `const char*` it does not own.
//
// Set from CMake per platform. The fallback is the older name on purpose: a
// build that forgets to define it is the Android/desktop app, which is the one
// that has always been called this.
#pragma once

#ifndef RETRO3DO_PRODUCT_NAME
#define RETRO3DO_PRODUCT_NAME "Retro-3DO"
#endif
