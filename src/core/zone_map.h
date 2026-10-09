#pragma once

#include <vector>

#include "core/types.h"

// 霸道產生的禁區（spec SZ2–SZ4、SX2）。同一格可以有多筆紀錄，各自計時。
class ZoneMap {
public:
    void add(Pos, PlayerId owner, TimeMs expiresAt);
    // pos 對 player 而言是否為禁區：存在 owner != player 且尚未到期（now < expiresAt）的紀錄
    bool isRestricted(Pos, PlayerId player, TimeMs now) const;
    void removeExpired(TimeMs now);
    void removeAt(Pos);                              // 摧毀時清掉該格的禁區（SX2）
    std::vector<ZoneCell> active(TimeMs now) const;  // 未到期的禁區，給 PlayerView（公開資訊，SZ4）
    void clear();

private:
    std::vector<ZoneCell> zones;
};
