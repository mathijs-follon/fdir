/*
 * Strong overrides for the fdir weak port hooks, wired to FreeRTOS primitives.
 * The C++ API uses a Port struct of lambdas instead; this file is not used by
 * the cxx example. It is kept as a reference for bare-metal C++ targets that
 * use the C hooks directly.
 *
 * The cxx example supplies port hooks via fdir::Port in main.cpp.
 */
