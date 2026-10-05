#define BLINK 1001
#define TALK 1002

// We know it will be the mouth here; each entry is openness, 0-100,
// followed by how many millis to spend there
int talk_map[5][2] = {
  { 50, 100 },
  { 10, 100 },
  { 30, 100 },
  { 10, 100 },
  { 50, 100 }
};
