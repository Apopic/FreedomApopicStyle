// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== Discord (ステータス表示・参加ボタン)
    // 画面ごとの状態を Discord のプロフィールに表示し、オンラインのルームにいる間はサーバーの「アドレス:ポート」を「参加」用の秘密として渡す。
    // フレンドが「参加」を押すと、その人のゲームにこの秘密が届き、そのサーバーに接続する。
    struct DiscordState {
        std::chrono::steady_clock::time_point NextUpdate{};
        Scene LastScene = Scene::Title;
        int64_t PlayStart = 0;
#ifndef __ANDROID__
        std::vector<DiscordRpc::Event> Events;
        std::optional<DiscordRpc::Event> Request;       // 参加の希望 (待機室で Y / N)
#endif
        std::chrono::steady_clock::time_point RequestUntil{};
    } Discord;

    // 使うアプリケーション ID (Config で無効なら空 = 停止)
    std::string DiscordAppId() const;
    // Discord の文字列は 2～128 バイト (UTF-8 の途中では切らない)
    static std::string DiscordText(std::string text);
    // 他の人に渡す接続先: 外部 (インターネット) のものがあればそれ、無ければ LAN
    const fasnet::AddressEntry* DiscordJoinAddress() const;
    void DiscordUpdate();
#endif // FAS_GAME_MEMBER_DECLARATIONS
