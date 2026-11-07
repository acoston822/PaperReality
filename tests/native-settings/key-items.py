#!/usr/bin/env python3
"""Exercise the production native Key Items page without game assets or a renderer."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/port/ui/devtools/saveeditor/SaveEditor.cpp').read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


letters = source[source.index('std::vector<LetterData> letterList ='):]
letters = letters[:letters.index('// The 63 star pieces')]
flags = sorted(set(re.findall(r'\bGF_\w+', letters)))
page = source.split('} else if (key == "save/key-items") {', 1)[1]
page = page.split('} else if (key == "save/letters") {', 1)[0]
assert '{ "Key Items", "key-items" }' in source, 'Key Items missing from native navigation'

harness = r'''
#include "port/ui/NativeSettings.h"
#include "item_enum.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Paperboat::NativeSettings;
constexpr int MAX_KEY_ITEM_SIZE = 32;
struct { int16_t keyItems[MAX_KEY_ITEM_SIZE] = {}; } gPlayerData;
intptr_t gItemIconRasterOffsets[349] = {};
bool loaded = true;
bool SaveLoaded() { return loaded; }
std::string ItemTitle(int id) { return "Item " + std::to_string(id); }
void ItemPortrait(Row& row, int id) {
    if (id <= ITEM_NONE || id > ITEM_UNUSED_PRISON_KEY_3) throw std::runtime_error("Invalid portrait ID");
    row.id = "item/" + std::to_string(id);
}
struct LetterData { const char* name; int itemID; int obtainedFlag = 0; bool returnedToParakarry = false; };
'''
harness += '\n'.join('#define ' + flag + ' 0' for flag in flags) + '\n' + letters
harness += '\n'.join(function(signature) for signature in (
    'bool PlayerHasKeyItem(', 'int32_t CountUsedKeyItemSlots(', 'void AddRemove_KeyItem(',
    'std::function<void()> EditSave(', 'std::function<void(double)> ChangeSave(', 'std::string KeyItemTitle('
))
harness += '\nPage BuildKeyItems() { Page page; if (!SaveLoaded()) return page;\n' + page + '\nreturn page; }\n'
harness += r'''
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Row Find(const Page& page, const std::string& id) {
    for (const auto& section : page.sections)
        for (const auto& row : section.rows)
            if (row.id == id) return row;
    throw std::runtime_error("Missing row: " + id);
}
int main() {
    for (int id = ITEM_NONE + 1; id <= ITEM_UNUSED_PRISON_KEY_3; ++id)
        gItemIconRasterOffsets[id] = reinterpret_cast<intptr_t>("icons/key/fixture");
    auto page = BuildKeyItems();
    Require(page.title == "Key Items" && page.searchable, "Missing native page metadata");
    Require(page.subtitle == "0 of 32 slots used", "Incorrect initial capacity");
    Require(page.sections[0].title == "Held Key Items (0/32)", "Capacity is missing from the visible UIKit section header");
    Require(page.sections[1].rows.size() == 127, "Catalog contains empty or non-key items");
    auto map = Find(page, "item/8");
    map.change(1);
    map.change(1);
    Require(CountUsedKeyItemSlots() == 1 && PlayerHasKeyItem(ITEM_MAP), "Duplicate add is not idempotent");
    auto staleRemoval = Find(BuildKeyItems(), "key-slot/0");
    gPlayerData.keyItems[0] = ITEM_BIG_MAP;
    staleRemoval.activate();
    Require(gPlayerData.keyItems[0] == ITEM_BIG_MAP, "Stale tap removed a replacement item");
    auto bigMap = Find(BuildKeyItems(), "item/9");
    for (auto& item : gPlayerData.keyItems) item = ITEM_MAP;
    bigMap.change(1);
    Require(!PlayerHasKeyItem(ITEM_BIG_MAP) && CountUsedKeyItemSlots() == 32, "Stale add overflowed capacity");
    page = BuildKeyItems();
    Require(!Find(page, "item/9").enabled && Find(page, "item/8").enabled, "Full inventory blocks removal");
    Find(page, "item/8").change(0);
    Require(CountUsedKeyItemSlots() == 31, "Toggle must remove exactly one held copy");
    Find(BuildKeyItems(), "item/9").change(1);
    Require(CountUsedKeyItemSlots() == 32 && PlayerHasKeyItem(ITEM_BIG_MAP), "Freed slot cannot be reused");
    auto remove = Find(BuildKeyItems(), "key-slot/0");
    loaded = false;
    remove.activate();
    map.change(0);
    Require(CountUsedKeyItemSlots() == 32, "Stale callback edited an unloaded save");
    Require(BuildKeyItems().sections.empty(), "Unloaded save exposed editing controls");
    loaded = true;
    map.change(std::numeric_limits<double>::quiet_NaN());
    Require(CountUsedKeyItemSlots() == 32, "Invalid input changed inventory");
    Require(KeyItemTitle(ITEM_MAP) == "Map" && KeyItemTitle(ITEM_BIG_MAP) == "Big Map", "Shared map icons are ambiguous");
    Require(KeyItemTitle(ITEM_LETTER_TO_MERLON) == "Letter to Merlon", "Individual letter name missing");
    Require(KeyItemTitle(ITEM_LETTER_CHAIN_GOOMPAPA_2) == "Chain Letter to Goompapa (2nd)", "Final chain letter name missing");
    Require(KeyItemTitle(ITEM_ARTIFACT) == ItemTitle(ITEM_ARTIFACT), "Artifact misidentified as a letter");
    Require(KeyItemTitle(ITEM_UNUSED_PRISON_KEY_3) == "Unused Prison Key 3", "Shared key icons are ambiguous");
    gPlayerData.keyItems[0] = 32767;
    page = BuildKeyItems();
    Require(Find(page, "key-slot/0").title == "Unknown Key Item (32767)", "Invalid held ID was not handled safely");
    std::cout << "Native Key Items: capacity, stale callbacks, names, navigation and invalid IDs passed.\n";
}
'''
with tempfile.TemporaryDirectory(prefix='paperreality-key-items-') as temp:
    temp = Path(temp)
    cpp = temp / 'check.cpp'
    cpp.write_text(harness)
    exe = temp / 'check'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra',
                    '-I' + str(root / 'src'), '-I' + str(root / 'include'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
