// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2017-2021 The MARIA developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MARIA_UINT256_H
#define MARIA_UINT256_H

#include "crypto/common.h"

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
protected:
    static constexpr int WIDTH = BITS / 8;
    uint8_t m_data[WIDTH];
public:
    /* construct 0 value by default */
    constexpr base_blob() : m_data() {}

    /* constructor for constants between 1 and 255 */
    constexpr explicit base_blob(uint8_t v) : m_data{v} {}

    explicit base_blob(const std::vector<unsigned char>& vch);

    bool IsNull() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (m_data[i] != 0)
                return false;
        return true;
    }

    void SetNull()
    {
        memset(m_data, 0, sizeof(m_data));
    }

    inline int Compare(const base_blob& other) const { return memcmp(m_data, other.m_data, sizeof(m_data)); }

    friend inline bool operator==(const base_blob& a, const base_blob& b) { return a.Compare(b) == 0; }
    friend inline bool operator!=(const base_blob& a, const base_blob& b) { return a.Compare(b) != 0; }
    friend inline bool operator<(const base_blob& a, const base_blob& b) { return a.Compare(b) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    const unsigned char* data() const { return m_data; }
    unsigned char* data() { return m_data; }

    unsigned char* begin()
    {
        return &m_data[0];
    }

    unsigned char* end()
    {
        return &m_data[WIDTH];
    }

    const unsigned char* begin() const
    {
        return &m_data[0];
    }

    const unsigned char* end() const
    {
        return &m_data[WIDTH];
    }

    unsigned int size() const
    {
        return sizeof(m_data);
    }

    uint64_t GetUint64(int pos) const
    {
        const uint8_t* ptr = m_data + pos * 8;
        return ((uint64_t)ptr[0]) | \
               ((uint64_t)ptr[1]) << 8 | \
               ((uint64_t)ptr[2]) << 16 | \
               ((uint64_t)ptr[3]) << 24 | \
               ((uint64_t)ptr[4]) << 32 | \
               ((uint64_t)ptr[5]) << 40 | \
               ((uint64_t)ptr[6]) << 48 | \
               ((uint64_t)ptr[7]) << 56;
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        s.write((char*)m_data, sizeof(m_data));
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        s.read((char*)m_data, sizeof(m_data));
    }
};

/** 88-bit opaque blob.
 */
class blob88 : public base_blob<88> {
public:
    blob88() {}
    blob88(const base_blob<88>& b) : base_blob<88>(b) {}
    explicit blob88(const std::vector<unsigned char>& vch) : base_blob<88>(vch) {}
};

/** 160-bit opaque blob.
 * @note This type is called uint160 for historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
    uint160() {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class uint256 : public base_blob<256> {
public:
    uint256() {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}

    /** A cheap hash function that just returns 64 bits from the result, it can be
     * used when the contents are considered uniformly random. It is not appropriate
     * when the value can easily be influenced from outside as e.g. a network adversary could
     * provide values to trigger worst-case behavior.
     * @note The result of this function is not stable between little and big endian.
     */
    uint64_t GetCheapHash() const { return ReadLE64(begin()); }

};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching UINT256_ZERO.
 */
inline uint256 uint256S(const char* str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching UINT256_ZERO via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

/** constant uint256 instances */
const uint256 UINT256_ZERO = uint256();
const uint256 UINT256_ONE = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
const uint256 UINT256_MAX = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

/** 512-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class uint512 : public base_blob<512> {
public:
    uint512() {}
    uint512(const base_blob<512>& b) : base_blob<512>(b) {}
    explicit uint512(const std::vector<unsigned char>& vch) : base_blob<512>(vch) {}
};

/* uint512 from const char *.
 * This is a separate function because the constructor uint512(const char*) can result
 * in dangerously catching uint512(0).
 */
inline uint512 uint512S(const char* str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}
/* uint512 from std::string.
 * This is a separate function because the constructor uint512(const std::string &str) can result
 * in dangerously catching uint512(0) via std::string(const char*).
 */
inline uint512 uint512S(const std::string& str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}

namespace std {
    template <>
    struct hash<uint256>
    {
        std::size_t operator()(const uint256& k) const
        {
            return (std::size_t)k.GetCheapHash();
        }
    };
}

#endif // MARIA_UINT256_H
