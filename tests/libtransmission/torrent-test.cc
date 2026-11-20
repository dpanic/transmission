// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <future>
#include <vector>

#include <libtransmission/transmission.h>

#include <libtransmission/bitfield.h>
#include <libtransmission/file.h>
#include <libtransmission/torrent.h>

#include "test-fixtures.h"

using TorrentTest = libtransmission::test::SessionTest;

namespace
{
auto constexpr TorFilenames = std::array{
    "Android-x86 8.1 r6 iso.torrent"sv,
    "debian-11.2.0-amd64-DVD-1.iso.torrent"sv,
    "ubuntu-18.04.6-desktop-amd64.iso.torrent"sv,
    "ubuntu-20.04.4-desktop-amd64.iso.torrent"sv,
};
}

TEST_F(TorrentTest, queueMoveUp)
{
    static constexpr auto ExpectedQueuePosition = std::array{ 0, 1, 3, 2 };
    auto ctor = tr_ctor{ session_ };
    auto torrents = std::array<tr_torrent*, TorFilenames.size()>{};
    std::transform(
        TorFilenames.begin(),
        TorFilenames.end(),
        torrents.begin(),
        [this](auto const filename) { return torrentInitFromFile(filename); });
    auto const move_torrents = std::array{ torrents[0], torrents[1], torrents[3] };

    // Pre-test sanity checks
    for (size_t i = 0; i < torrents.size(); ++i)
    {
        ASSERT_EQ(i, torrents[i]->queue_position());
        ASSERT_EQ(i + 1U, torrents[i]->id());
    }

    tr_torrentsQueueMoveUp(move_torrents.data(), move_torrents.size());

    for (size_t i = 0; i < ExpectedQueuePosition.size(); ++i)
    {
        EXPECT_EQ(ExpectedQueuePosition[i], torrents[i]->queue_position()) << i;
    }
}

TEST_F(TorrentTest, queueMoveDown)
{
    static constexpr auto ExpectedQueuePosition = std::array{ 1, 0, 2, 3 };
    auto ctor = tr_ctor{ session_ };
    auto torrents = std::array<tr_torrent*, TorFilenames.size()>{};
    std::transform(
        TorFilenames.begin(),
        TorFilenames.end(),
        torrents.begin(),
        [this](auto const filename) { return torrentInitFromFile(filename); });
    auto const move_torrents = std::array{ torrents[0], torrents[2], torrents[3] };

    // Pre-test sanity checks
    for (size_t i = 0; i < torrents.size(); ++i)
    {
        ASSERT_EQ(i, torrents[i]->queue_position());
        ASSERT_EQ(i + 1U, torrents[i]->id());
    }

    tr_torrentsQueueMoveDown(move_torrents.data(), move_torrents.size());

    for (size_t i = 0; i < ExpectedQueuePosition.size(); ++i)
    {
        EXPECT_EQ(ExpectedQueuePosition[i], torrents[i]->queue_position()) << i;
    }
}

TEST_F(TorrentTest, useSessionLimitsAffectsBothDirections)
{
    auto* const tor = torrentInitFromFile(TorFilenames[0]);
    ASSERT_NE(nullptr, tor);

    EXPECT_TRUE(tor->bandwidth().are_parent_limits_honored(TR_UP));
    EXPECT_TRUE(tor->bandwidth().are_parent_limits_honored(TR_DOWN));

    tr_torrentUseSessionLimits(tor, false);
    EXPECT_FALSE(tor->bandwidth().are_parent_limits_honored(TR_UP));
    EXPECT_FALSE(tor->bandwidth().are_parent_limits_honored(TR_DOWN));

    tr_torrentUseSessionLimits(tor, true);
    EXPECT_TRUE(tor->bandwidth().are_parent_limits_honored(TR_UP));
    EXPECT_TRUE(tor->bandwidth().are_parent_limits_honored(TR_DOWN));
}

TEST_F(TorrentTest, queueMoveTop)
{
    static constexpr auto ExpectedQueuePosition = std::array{ 0, 3, 1, 2 };
    auto ctor = tr_ctor{ session_ };
    auto torrents = std::array<tr_torrent*, TorFilenames.size()>{};
    std::transform(
        TorFilenames.begin(),
        TorFilenames.end(),
        torrents.begin(),
        [this](auto const filename) { return torrentInitFromFile(filename); });
    auto const move_torrents = std::array{ torrents[0], torrents[2], torrents[3] };

    // Pre-test sanity checks
    for (size_t i = 0; i < torrents.size(); ++i)
    {
        ASSERT_EQ(i, torrents[i]->queue_position());
        ASSERT_EQ(i + 1U, torrents[i]->id());
    }

    tr_torrentsQueueMoveTop(move_torrents.data(), move_torrents.size());

    for (size_t i = 0; i < ExpectedQueuePosition.size(); ++i)
    {
        EXPECT_EQ(ExpectedQueuePosition[i], torrents[i]->queue_position()) << i;
    }
}

TEST_F(TorrentTest, queueMoveBottom)
{
    static constexpr auto ExpectedQueuePosition = std::array{ 1, 2, 0, 3 };
    auto ctor = tr_ctor{ session_ };
    auto torrents = std::array<tr_torrent*, TorFilenames.size()>{};
    std::transform(
        TorFilenames.begin(),
        TorFilenames.end(),
        torrents.begin(),
        [this](auto const filename) { return torrentInitFromFile(filename); });
    auto const move_torrents = std::array{ torrents[0], torrents[1], torrents[3] };

    // Pre-test sanity checks
    for (size_t i = 0; i < torrents.size(); ++i)
    {
        ASSERT_EQ(i, torrents[i]->queue_position());
        ASSERT_EQ(i + 1U, torrents[i]->id());
    }

    tr_torrentsQueueMoveBottom(move_torrents.data(), move_torrents.size());

    for (size_t i = 0; i < ExpectedQueuePosition.size(); ++i)
    {
        EXPECT_EQ(ExpectedQueuePosition[i], torrents[i]->queue_position()) << i;
    }
}

namespace libtransmission::test
{

TEST_F(TorrentTest, pieceThatFailsItsLazyCheckIsWantedAgain)
{
    static auto constexpr GoodPiece = tr_piece_index_t{ 1U };
    static auto constexpr BadPiece = tr_piece_index_t{ 2U };

    auto* const tor = zeroTorrentInit(ZeroTorrentState::Complete);
    ASSERT_NE(nullptr, tor);
    tr_torrentStartNow(tor);
    ASSERT_TRUE(waitFor([tor] { return tr_torrentStat(tor)->activity == TR_STATUS_SEED; }, 5s));

    // the last piece is the only one that holds the last file
    auto const unreadable_piece = tor->piece_count() - 1U;
    auto const last_file = tor->file_count() - 1U;
    ASSERT_EQ(unreadable_piece, tor->piece_span_for_file(last_file).begin);

    // change a piece behind the torrent's back
    static auto constexpr Garbage = "garbage"sv;
    auto const fd = tr_sys_file_open(tr_torrentFindFile(tor, 0U).c_str(), TR_SYS_FILE_WRITE, 0600);
    ASSERT_NE(TR_BAD_SYS_FILE, fd);
    EXPECT_TRUE(tr_sys_file_write_at(fd, std::data(Garbage), std::size(Garbage), tor->piece_loc(BadPiece).byte, nullptr));
    tr_sys_file_close(fd);

    // make another piece unreadable
    EXPECT_TRUE(tr_sys_path_remove(tr_torrentFindFile(tor, last_file).c_str()));

    auto lost_pieces = std::vector<tr_piece_index_t>{};
    auto done = std::promise<void>{};
    session_->run_in_session_thread(
        [&]()
        {
            auto const tag = tor->lost_piece_.observe([&lost_pieces](tr_torrent*, tr_piece_index_t piece)
                                                      { lost_pieces.push_back(piece); });

            // the pieces are untested again, as they are after loading
            // a resume file whose mtimes do not match the files on disk
            auto const mtimes = std::vector<time_t>(tor->file_count());
            tr_torrent::ResumeHelper{ *tor }.load_checked_pieces(tr_bitfield{ tor->piece_count() }, std::data(mtimes));

            EXPECT_TRUE(tor->ensure_piece_is_checked(GoodPiece));
            EXPECT_TRUE(tor->has_piece(GoodPiece));

            EXPECT_TRUE(tor->has_piece(BadPiece));
            EXPECT_FALSE(tor->ensure_piece_is_checked(BadPiece));
            EXPECT_FALSE(tor->has_piece(BadPiece));

            // a piece that can't be read is not given up on:
            // its file may come back, e.g. when a drive is reconnected
            EXPECT_FALSE(tor->ensure_piece_is_checked(unreadable_piece));
            EXPECT_TRUE(tor->has_piece(unreadable_piece));

            done.set_value();
        });
    ASSERT_EQ(std::future_status::ready, done.get_future().wait_for(5s));

    // the bad piece is no longer counted as local data, so the torrent downloads it again
    EXPECT_EQ(std::vector{ BadPiece }, lost_pieces);
    EXPECT_TRUE(waitFor([tor] { return tr_torrentStat(tor)->activity == TR_STATUS_DOWNLOAD; }, 5s));
    EXPECT_EQ(tor->piece_size(BadPiece), tr_torrentStat(tor)->leftUntilDone);

    tr_torrentRemove(tor, false, nullptr, nullptr);
}

} // namespace libtransmission::test
