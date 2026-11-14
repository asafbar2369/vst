#include "ParamArea.h"
#include <cassert>
#include <iostream>

int main()
{
    ParamArea::Spec spec;
    spec.size = spec.slotStride = 4;
    spec.numSlots = 2;
    spec.bankBase = 0x8000;
    spec.minFor = [] (int) { return 0; };
    spec.maxFor = [] (int) { return 127; };
    spec.writeInit = [] (uint8_t* p) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t) (10 + i); };
    ParamArea a (spec);
    assert (! a.isSlotValid (0) && ! a.isSlotComplete (0));
    assert (! a.isSlotValid (-1) && ! a.isSlotValid (2));
    assert (! a.isAddressRange (0xffffffffu - 1, 4));
    assert (a.write (spec.bankBase, 23));
    assert (a.isSlotValid (0) && ! a.isSlotComplete (0));
    assert (a.getBank (0, 0) == 23);
    a.write (spec.bankBase, 23);  // duplicate address cannot inflate coverage
    assert (! a.isSlotComplete (0));
    a.write (spec.bankBase + 1, 22);
    a.write (spec.bankBase + 3, 20);
    assert (! a.isSlotComplete (0));
    a.write (spec.bankBase + 2, 21);
    assert (a.isSlotComplete (0));
    assert (! a.isSlotValid (1));
    uint8_t data[] = { 4, 3, 2, 1 };
    a.loadSlot (1, data);
    assert (a.isSlotComplete (1) && a.getBank (1, 3) == 1);
    a.clearBank();
    assert (! a.isSlotValid (0) && ! a.isSlotValid (1));
    assert (a.getBank (0, 0) == 10 && a.getBank (1, 3) == 13);
    a.write (spec.bankBase + 2, 99);
    assert (a.isSlotValid (0) && ! a.isSlotComplete (0));
    assert (a.getBank (0, 0) == 10); // cannot inherit a previous project's bank byte
    uint8_t saved[4], seen[4];
    for (int i = 0; i < 4; ++i)
    {
        saved[i] = a.getBank (0, i);
        seen[i] = a.isBankByteSeen (0, i) ? 1 : 0;
    }
    assert (seen[0] == 0 && seen[1] == 0 && seen[2] == 1 && seen[3] == 0);
    ParamArea restored (spec);
    restored.loadSlotWithCoverage (0, saved, seen);
    assert (restored.isSlotValid (0) && ! restored.isSlotComplete (0));
    assert (restored.getBank (0, 2) == 99 && ! restored.isBankByteSeen (0, 0));
    assert (restored.write (spec.bankBase, 23) && ! restored.isSlotComplete (0));
    assert (restored.write (spec.bankBase + 1, 24) && ! restored.isSlotComplete (0));
    assert (restored.write (spec.bankBase + 3, 25) && restored.isSlotComplete (0));
    uint8_t malformed[4] = { 0, 0, 0, 0 };
    restored.loadSlotWithCoverage (0, saved, malformed);
    assert (! restored.isSlotValid (0) && ! restored.isSlotComplete (0));
    std::cout << "PASS: partial/full bank coverage, save/restore and overflow guards\n";
}
