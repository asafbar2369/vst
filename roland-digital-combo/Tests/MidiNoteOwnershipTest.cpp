#include "MidiNoteOwnership.h"
#include <cassert>
#include <iostream>

int main()
{
    dc::midi::NoteMap map;
    map.fill (-1);
    assert (! dc::midi::hasOwner (map, 60));
    map[60] = 72; // Source channel 1, C4 -> C5 after transpose
    map[61] = 72; // Two different source notes map to the same C5
    assert (dc::midi::hasOwner (map, 72, 1));
    map[60] = -1; // First note-off must not stop the second held note
    assert (dc::midi::hasOwner (map, 72, 1));
    map[61] = -1;
    assert (! dc::midi::hasOwner (map, 72, 1));

    map[128 + 60] = 72; // Source channel 2 owns same pitch
    assert (dc::midi::hasOwner (map, 72));
    assert (! dc::midi::hasOwner (map, 72, 1));
    assert (dc::midi::hasOwner (map, 72, 2));
    map[128 + 60] = -1;
    assert (! dc::midi::hasOwner (map, 72));

    assert (! dc::midi::hasOwner (map, -1));
    assert (! dc::midi::hasOwner (map, 128));
    assert (! dc::midi::hasOwner (map, 72, 17));
    std::cout << "PASS: held-note ownership across source keys and MIDI channels\n";
}
