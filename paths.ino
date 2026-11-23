// paths — builds the light order for each path.
//
// Whole house: the strands leave the controller in a Y, so starting at the far end
// of B and running back to the controller, then out along A, sweeps one end of the
// house to the other:
//   path position 0..64   -> B65 .. B1  (B index 64 .. 0)
//   path position 65..129 -> A1  .. A65 (A index 0 .. 64)

void initPaths() {
  int p = 0;
  for (int i = NUM_LEDS - 1; i >= 0; i--) housePath[p++] = { 1, (uint16_t)i };  // B65 -> B1
  for (int i = 0; i < NUM_LEDS; i++)      housePath[p++] = { 0, (uint16_t)i };  // A1 -> A65
}
