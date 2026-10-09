#include "core/zone_map.h"

#include <algorithm>
#include <iterator>

void ZoneMap::add(Pos pos, PlayerId owner, TimeMs expiresAt) {
    zones.push_back(ZoneCell{pos, owner, expiresAt});
}

bool ZoneMap::isRestricted(Pos pos, PlayerId player, TimeMs now) const {
    return std::any_of(zones.begin(), zones.end(), [&](const ZoneCell& z) {
        return z.pos == pos && z.owner != player && now < z.expiresAt;
    });
}

void ZoneMap::removeExpired(TimeMs now) {
    zones.erase(std::remove_if(zones.begin(), zones.end(), [now](const ZoneCell& z) { return now >= z.expiresAt; }),
                zones.end());
}

void ZoneMap::removeAt(Pos pos) {
    zones.erase(std::remove_if(zones.begin(), zones.end(), [pos](const ZoneCell& z) { return z.pos == pos; }),
                zones.end());
}

std::vector<ZoneCell> ZoneMap::active(TimeMs now) const {
    std::vector<ZoneCell> result;
    std::copy_if(zones.begin(), zones.end(), std::back_inserter(result),
                 [now](const ZoneCell& z) { return now < z.expiresAt; });
    return result;
}

void ZoneMap::clear() {
    zones.clear();
}
