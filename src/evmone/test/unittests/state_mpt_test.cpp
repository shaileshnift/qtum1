// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/mpt.hpp>
#include <test/state/rlp.hpp>
#include <test/utils/utils.hpp>
#include <numeric>

using namespace evmone;
using namespace evmone::state;
using namespace intx;

TEST(state_mpt, empty_trie)
{
    EXPECT_EQ(MPT{}.hash(), emptyMPTHash);
}

TEST(state_mpt, single_account_v1)
{
    // Expected value computed in go-ethereum.
    constexpr auto expected =
        0x084f337237951e425716a04fb0aaa74111eda9d9c61767f2497697d0a201c92e_bytes32;

    constexpr auto addr = 0x0000000000000000000000000000000000000002_address;
    constexpr uint64_t nonce = 0;
    constexpr auto balance = 1_u256;
    constexpr auto storage_hash = emptyMPTHash;
    constexpr auto code_hash =
        0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470_bytes32;

    MPT trie;
    const auto xkey = keccak256(addr);
    auto xvalue = rlp::encode_tuple(nonce, balance, storage_hash, code_hash);
    trie.insert(xkey, std::move(xvalue));
    EXPECT_EQ(trie.hash(), expected);
}

TEST(state_mpt, storage_trie_v1)
{
    constexpr auto expected =
        0xd9aa83255221f68fdd4931f73f8fe6ea30c191a9619b5fc60ce2914eee1e7e54_bytes32;

    const auto key = 0x00_bytes32;
    const auto value = 0x01ff_bytes32;
    const auto xkey = keccak256(key);
    auto xvalue = rlp::encode(rlp::trim(value));

    MPT trie;
    trie.insert(xkey, std::move(xvalue));
    EXPECT_EQ(trie.hash(), expected);
}

TEST(state_mpt, leaf_node_example1)
{
    MPT trie;
    trie.insert("010203"_hex, "hello"_b);
    EXPECT_EQ(hex(trie.hash()), "82c8fd36022fbc91bd6b51580cfd941d3d9994017d59ab2e8293ae9c94c3ab6e");
}

TEST(state_mpt, branch_node_example1)
{
    // A trie of single branch node and two leaf nodes with paths of length 2.
    // The branch node has leaf nodes at positions [4] and [5].
    // {4:1, 5:a}

    auto value1 = "v___________________________1"_b;
    const auto key1 = "41"_hex;
    const uint8_t path1[]{0x4, 0x1};
    const bytes encoded_path1{static_cast<uint8_t>(0x30 | path1[1])};
    const auto leaf_node1 = rlp::encode_tuple(encoded_path1, value1);
    EXPECT_EQ(hex(leaf_node1), "df319d765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f31");

    auto value2 = "v___________________________2"_b;
    const auto key2 = "5a"_hex;
    const uint8_t path2[]{0x5, 0xa};
    const bytes encoded_path2{static_cast<uint8_t>(0x30 | path2[1])};
    const auto leaf_node2 = rlp::encode_tuple(encoded_path2, value2);
    EXPECT_EQ(hex(leaf_node2), "df3a9d765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f32");

    MPT trie;
    trie.insert(key1, std::move(value1));
    trie.insert(key2, std::move(value2));
    EXPECT_EQ(hex(trie.hash()), "1aaa6f712413b9a115730852323deb5f5d796c29151a60a1f55f41a25354cd26");
}

TEST(state_mpt, extension_node_example1)
{
    // A trie of an extension node followed by a branch node with
    // two leafs with single nibble paths.
    // 5858:{4:1, 5:a}

    auto value1 = "v___________________________1"_b;
    const auto key1 = "585841"_hex;
    [[maybe_unused]] const uint8_t path1[]{0x5, 0x8, 0x5, 0x8, 0x4, 0x1};

    auto value2 = "v___________________________2"_b;
    const auto key2 = "58585a"_hex;
    [[maybe_unused]] const uint8_t path2[]{0x5, 0x8, 0x5, 0x8, 0x5, 0xa};

    const bytes encoded_common_path{0x00, 0x58, 0x58};

    // The hash of the branch node. See the branch_node_example test.
    constexpr auto branch_node_hash =
        0x1aaa6f712413b9a115730852323deb5f5d796c29151a60a1f55f41a25354cd26_bytes32;

    const auto extension_node = rlp::encode_tuple(encoded_common_path, branch_node_hash);
    EXPECT_EQ(hex(keccak256(extension_node)),
        "3eefc183db443d44810b7d925684eb07256e691d5c9cb13215660107121454f9");

    MPT trie;
    trie.insert(key1, std::move(value1));
    trie.insert(key2, std::move(value2));
    EXPECT_EQ(hex(trie.hash()), "3eefc183db443d44810b7d925684eb07256e691d5c9cb13215660107121454f9");
}

TEST(state_mpt, extension_node_example2)
{
    // A trie of an extension node followed by a branch node with
    // two leafs with longer paths.
    // 585:{8:41, 9:5a}

    auto value1 = "v___________________________1"_b;
    const auto key1 = "XXA"_b;
    const uint8_t path1[]{0x5, 0x8, 0x5, 0x8, 0x4, 0x1};

    auto value2 = "v___________________________2"_b;
    const auto key2 = "XYZ"_b;
    const uint8_t path2[]{0x5, 0x8, 0x5, 0x9, 0x5, 0xa};

    const uint8_t common_path[]{0x5, 0x8, 0x5};
    const bytes encoded_path1{0x20, static_cast<uint8_t>((path1[4] << 4) | path1[5])};
    EXPECT_EQ(hex(encoded_path1), "2041");
    const bytes encoded_path2{0x20, static_cast<uint8_t>((path2[4] << 4) | path2[5])};
    EXPECT_EQ(hex(encoded_path2), "205a");

    const auto node1 = rlp::encode_tuple(encoded_path1, value1);
    EXPECT_EQ(hex(node1), "e18220419d765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f31");
    const auto node2 = rlp::encode_tuple(encoded_path2, value2);
    EXPECT_EQ(hex(node2), "e182205a9d765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f32");

    constexpr auto branch_node_hash =
        0x01746f8ab5a4cc5d6175cbd9ea9603357634ec06b2059f90710243f098e0ee82_bytes32;

    const bytes encoded_common_path{static_cast<uint8_t>(0x10 | common_path[0]),
        static_cast<uint8_t>((common_path[1] << 4) | common_path[2])};
    const auto extension_node = rlp::encode_tuple(encoded_common_path, branch_node_hash);
    EXPECT_EQ(hex(keccak256(extension_node)),
        "ac28c08fa3ff1d0d2cc9a6423abb7af3f4dcc37aa2210727e7d3009a9b4a34e8");

    MPT trie;
    trie.insert(key1, std::move(value1));
    trie.insert(key2, std::move(value2));
    EXPECT_EQ(hex(trie.hash()), "ac28c08fa3ff1d0d2cc9a6423abb7af3f4dcc37aa2210727e7d3009a9b4a34e8");
}

TEST(state_mpt, trie_topologies)
{
    struct KVH
    {
        const char* key_hex;
        const char* value;
        const char* hash_hex;
    };

    // clang-format off
    const std::vector<KVH> tests[] = {
        { // {0:0, 7:0, f:0}
            {"00", "v_______________________0___0", "5cb26357b95bb9af08475be00243ceb68ade0b66b5cd816b0c18a18c612d2d21"},
            {"70", "v_______________________0___1", "8ff64309574f7a437a7ad1628e690eb7663cfde10676f8a904a8c8291dbc1603"},
            {"f0", "v_______________________0___2", "9e3a01bd8d43efb8e9d4b5506648150b8e3ed1caea596f84ee28e01a72635470"},
        },
        { // {1:0cc, e:{1:fc, e:fc}}
            {"10cc", "v_______________________1___0", "233e9b257843f3dfdb1cce6676cdaf9e595ac96ee1b55031434d852bc7ac9185"},
            {"e1fc", "v_______________________1___1", "39c5e908ae83d0c78520c7c7bda0b3782daf594700e44546e93def8f049cca95"},
            {"eefc", "v_______________________1___2", "d789567559fd76fe5b7d9cc42f3750f942502ac1c7f2a466e2f690ec4b6c2a7c"},
        },
        { // {1:0cc, e:{1:fc, e:fc}}
            {"10cc", "v_______________________1___0", "233e9b257843f3dfdb1cce6676cdaf9e595ac96ee1b55031434d852bc7ac9185"},
            {"e1fc", "v_______________________1___1", "39c5e908ae83d0c78520c7c7bda0b3782daf594700e44546e93def8f049cca95"},
            {"eefc", "v_______________________1___2", "d789567559fd76fe5b7d9cc42f3750f942502ac1c7f2a466e2f690ec4b6c2a7c"},
        },
        { // {b:{a:ac, b:ac}, d:acc}
            {"baac", "v_______________________2___0", "8be1c86ba7ec4c61e14c1a9b75055e0464c2633ae66a055a24e75450156a5d42"},
            {"bbac", "v_______________________2___1", "8495159b9895a7d88d973171d737c0aace6fe6ac02a4769fff1bc43bcccce4cc"},
            {"dacc", "v_______________________2___2", "9bcfc5b220a27328deb9dc6ee2e3d46c9ebc9c69e78acda1fa2c7040602c63ca"},
        },
        { // {0:0cccc, 2:456{0:0, 2:2}
            {"00cccc", "v_______________________3___0", "e57dc2785b99ce9205080cb41b32ebea7ac3e158952b44c87d186e6d190a6530"},
            {"245600", "v_______________________3___1", "0335354adbd360a45c1871a842452287721b64b4234dfe08760b243523c998db"},
            {"245622", "v_______________________3___2", "9e6832db0dca2b5cf81c0e0727bfde6afc39d5de33e5720bccacc183c162104e"},
        },
        { // {1:4567{1:1c, 3:3c}, 3:0cccccc}
            {"1456711c", "v_______________________4___0", "f2389e78d98fed99f3e63d6d1623c1d4d9e8c91cb1d585de81fbc7c0e60d3529"},
            {"1456733c", "v_______________________4___1", "101189b3fab852be97a0120c03d95eefcf984d3ed639f2328527de6def55a9c0"},
            {"30cccccc", "v_______________________4___2", "3780ce111f98d15751dfde1eb21080efc7d3914b429e5c84c64db637c55405b3"},
        },
        { // 8800{1:f, 2:e, 3:d}
            {"88001f", "v_______________________5___0", "e817db50d84f341d443c6f6593cafda093fc85e773a762421d47daa6ac993bd5"},
            {"88002e", "v_______________________5___1", "d6e3e6047bdc110edd296a4d63c030aec451bee9d8075bc5a198eee8cda34f68"},
            {"88003d", "v_______________________5___2", "b6bdf8298c703342188e5f7f84921a402042d0e5fb059969dd53a6b6b1fb989e"},
        },
        { // 0{1:fc, 2:ec, 4:dc}
            {"01fc", "v_______________________6___0", "693268f2ca80d32b015f61cd2c4dba5a47a6b52a14c34f8e6945fad684e7a0d5"},
            {"02ec", "v_______________________6___1", "e24ddd44469310c2b785a2044618874bf486d2f7822603a9b8dce58d6524d5de"},
            {"04dc", "v_______________________6___2", "33fc259629187bbe54b92f82f0cd8083b91a12e41a9456b84fc155321e334db7"},
        },
        { // f{0:fccc, f:ff{0:f, f:f}}
            {"f0fccc", "v_______________________7___0", "b0966b5aa469a3e292bc5fcfa6c396ae7a657255eef552ea7e12f996de795b90"},
            {"ffff0f", "v_______________________7___1", "3b1ca154ec2a3d96d8d77bddef0abfe40a53a64eb03cecf78da9ec43799fa3d0"},
            {"ffffff", "v_______________________7___2", "e75463041f1be8252781be0ace579a44ea4387bf5b2739f4607af676f7719678"},
        },
        { // ff{0:f{0:f, f:f}, f:fcc}
            {"ff0f0f", "v_______________________8___0", "0928af9b14718ec8262ab89df430f1e5fbf66fac0fed037aff2b6767ae8c8684"},
            {"ff0fff", "v_______________________8___1", "d870f4d3ce26b0bf86912810a1960693630c20a48ba56be0ad04bc3e9ddb01e6"},
            {"ffffcc", "v_______________________8___2", "4239f10dd9d9915ecf2e047d6a576bdc1733ed77a30830f1bf29deaf7d8e966f"},
        },
        {
            {"123d", "x___________________________0", "fc453d88b6f128a77c448669710497380fa4588abbea9f78f4c20c80daa797d0"},
            {"123e", "x___________________________1", "5af48f2d8a9a015c1ff7fa8b8c7f6b676233bd320e8fb57fd7933622badd2cec"},
            {"123f", "x___________________________2", "1164d7299964e74ac40d761f9189b2a3987fae959800d0f7e29d3aaf3eae9e15"},
        },
        {
            {"123d", "x___________________________0", "fc453d88b6f128a77c448669710497380fa4588abbea9f78f4c20c80daa797d0"},
            {"123e", "x___________________________1", "5af48f2d8a9a015c1ff7fa8b8c7f6b676233bd320e8fb57fd7933622badd2cec"},
            {"124a", "x___________________________2", "661a96a669869d76b7231380da0649d013301425fbea9d5c5fae6405aa31cfce"},
        },
        {
            {"123d", "x___________________________0", "fc453d88b6f128a77c448669710497380fa4588abbea9f78f4c20c80daa797d0"},
            {"123e", "x___________________________1", "5af48f2d8a9a015c1ff7fa8b8c7f6b676233bd320e8fb57fd7933622badd2cec"},
            {"13aa", "x___________________________2", "6590120e1fd3ffd1a90e8de5bb10750b61079bb0776cca4414dd79a24e4d4356"},
        },
        {
            {"123d", "x___________________________0", "fc453d88b6f128a77c448669710497380fa4588abbea9f78f4c20c80daa797d0"},
            {"123e", "x___________________________1", "5af48f2d8a9a015c1ff7fa8b8c7f6b676233bd320e8fb57fd7933622badd2cec"},
            {"2aaa", "x___________________________2", "f869b40e0c55eace1918332ef91563616fbf0755e2b946119679f7ef8e44b514"},
        },
        {
            {"1234da", "x___________________________0", "1c4b4462e9f56a80ca0f5d77c0d632c41b0102290930343cf1791e971a045a79"},
            {"1234ea", "x___________________________1", "2f502917f3ba7d328c21c8b45ee0f160652e68450332c166d4ad02d1afe31862"},
            {"1234fa", "x___________________________2", "4f4e368ab367090d5bc3dbf25f7729f8bd60df84de309b4633a6b69ab66142c0"},
        },
        {
            {"1234da", "x___________________________0", "1c4b4462e9f56a80ca0f5d77c0d632c41b0102290930343cf1791e971a045a79"},
            {"1234ea", "x___________________________1", "2f502917f3ba7d328c21c8b45ee0f160652e68450332c166d4ad02d1afe31862"},
            {"1235aa", "x___________________________2", "21840121d11a91ac8bbad9a5d06af902a5c8d56a47b85600ba813814b7bfcb9b"},
        },
        {
            {"1234da", "x___________________________0", "1c4b4462e9f56a80ca0f5d77c0d632c41b0102290930343cf1791e971a045a79"},
            {"1234ea", "x___________________________1", "2f502917f3ba7d328c21c8b45ee0f160652e68450332c166d4ad02d1afe31862"},
            {"124aaa", "x___________________________2", "ea4040ddf6ae3fbd1524bdec19c0ab1581015996262006632027fa5cf21e441e"},
        },
        {
            {"1234da", "x___________________________0", "1c4b4462e9f56a80ca0f5d77c0d632c41b0102290930343cf1791e971a045a79"},
            {"1234ea", "x___________________________1", "2f502917f3ba7d328c21c8b45ee0f160652e68450332c166d4ad02d1afe31862"},
            {"13aaaa", "x___________________________2", "e4beb66c67e44f2dd8ba36036e45a44ff68f8d52942472b1911a45f886a34507"},
        },
        {
            {"1234da", "x___________________________0", "1c4b4462e9f56a80ca0f5d77c0d632c41b0102290930343cf1791e971a045a79"},
            {"1234ea", "x___________________________1", "2f502917f3ba7d328c21c8b45ee0f160652e68450332c166d4ad02d1afe31862"},
            {"2aaaaa", "x___________________________2", "5f5989b820ff5d76b7d49e77bb64f26602294f6c42a1a3becc669cd9e0dc8ec9"},
        },
        {
            {"000000", "x___________________________0", "3b32b7af0bddc7940e7364ee18b5a59702c1825e469452c8483b9c4e0218b55a"},
            {"1234da", "x___________________________1", "3ab152a1285dca31945566f872c1cc2f17a770440eda32aeee46a5e91033dde2"},
            {"1234ea", "x___________________________2", "0cccc87f96ddef55563c1b3be3c64fff6a644333c3d9cd99852cb53b6412b9b8"},
            {"1234fa", "x___________________________3", "65bb3aafea8121111d693ffe34881c14d27b128fd113fa120961f251fe28428d"},
        },
        {
            {"000000", "x___________________________0", "3b32b7af0bddc7940e7364ee18b5a59702c1825e469452c8483b9c4e0218b55a"},
            {"1234da", "x___________________________1", "3ab152a1285dca31945566f872c1cc2f17a770440eda32aeee46a5e91033dde2"},
            {"1234ea", "x___________________________2", "0cccc87f96ddef55563c1b3be3c64fff6a644333c3d9cd99852cb53b6412b9b8"},
            {"1235aa", "x___________________________3", "f670e4d2547c533c5f21e0045442e2ecb733f347ad6d29ef36e0f5ba31bb11a8"},
        },
        {
            {"000000", "x___________________________0", "3b32b7af0bddc7940e7364ee18b5a59702c1825e469452c8483b9c4e0218b55a"},
            {"1234da", "x___________________________1", "3ab152a1285dca31945566f872c1cc2f17a770440eda32aeee46a5e91033dde2"},
            {"1234ea", "x___________________________2", "0cccc87f96ddef55563c1b3be3c64fff6a644333c3d9cd99852cb53b6412b9b8"},
            {"124aaa", "x___________________________3", "c17464123050a9a6f29b5574bb2f92f6d305c1794976b475b7fb0316b6335598"},
        },
        {
            {"000000", "x___________________________0", "3b32b7af0bddc7940e7364ee18b5a59702c1825e469452c8483b9c4e0218b55a"},
            {"1234da", "x___________________________1", "3ab152a1285dca31945566f872c1cc2f17a770440eda32aeee46a5e91033dde2"},
            {"1234ea", "x___________________________2", "0cccc87f96ddef55563c1b3be3c64fff6a644333c3d9cd99852cb53b6412b9b8"},
            {"13aaaa", "x___________________________3", "aa8301be8cb52ea5cd249f5feb79fb4315ee8de2140c604033f4b3fff78f0105"},
        },
        {
            {"0000", "x___________________________0", "cb8c09ad07ae882136f602b3f21f8733a9f5a78f1d2525a8d24d1c13258000b2"},
            {"123d", "x___________________________1", "8f09663deb02f08958136410dc48565e077f76bb6c9d8c84d35fc8913a657d31"},
            {"123e", "x___________________________2", "0d230561e398c579e09a9f7b69ceaf7d3970f5a436fdb28b68b7a37c5bdd6b80"},
            {"123f", "x___________________________3", "80f7bad1893ca57e3443bb3305a517723a74d3ba831bcaca22a170645eb7aafb"},
        },
        {
            {"0000", "x___________________________0", "cb8c09ad07ae882136f602b3f21f8733a9f5a78f1d2525a8d24d1c13258000b2"},
            {"123d", "x___________________________1", "8f09663deb02f08958136410dc48565e077f76bb6c9d8c84d35fc8913a657d31"},
            {"123e", "x___________________________2", "0d230561e398c579e09a9f7b69ceaf7d3970f5a436fdb28b68b7a37c5bdd6b80"},
            {"124a", "x___________________________3", "383bc1bb4f019e6bc4da3751509ea709b58dd1ac46081670834bae072f3e9557"},
        },
        {
            {"0000", "x___________________________0", "cb8c09ad07ae882136f602b3f21f8733a9f5a78f1d2525a8d24d1c13258000b2"},
            {"123d", "x___________________________1", "8f09663deb02f08958136410dc48565e077f76bb6c9d8c84d35fc8913a657d31"},
            {"123e", "x___________________________2", "0d230561e398c579e09a9f7b69ceaf7d3970f5a436fdb28b68b7a37c5bdd6b80"},
            {"13aa", "x___________________________3", "ff0dc70ce2e5db90ee42a4c2ad12139596b890e90eb4e16526ab38fa465b35cf"},
        },
    };
    // clang-format on

    for (const auto& test : tests)
    {
        // Insert in order and check hash at every step.
        {
            MPT trie;
            for (const auto& kv : test)
            {
                trie.insert(from_hex(kv.key_hex).value(), to_bytes(kv.value));
                EXPECT_EQ(hex(trie.hash()), kv.hash_hex);
            }
        }

        // Check if all insert order permutations give the same final hash.
        std::vector<size_t> order(test.size());
        std::iota(order.begin(), order.end(), size_t{0});
        while (std::next_permutation(order.begin(), order.end()))
        {
            MPT trie;
            for (size_t i = 0; i < test.size(); ++i)
                trie.insert(
                    from_hex(test[order[i]].key_hex).value(), to_bytes(test[order[i]].value));
            EXPECT_EQ(hex(trie.hash()), test.back().hash_hex);
        }
    }
}
