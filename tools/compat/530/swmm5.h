/* 5.3.0 ships its toolkit header as openswmm_solver.h, a superset of 5.2.4's
 * swmm5.h. The review's legacy tests include "swmm5.h"; this shim lets the same
 * test compile against both versions. */
#include "openswmm_solver.h"
