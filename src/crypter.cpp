// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2017-2020 The MARIA developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "crypto/aes.h"
#include "crypto/sha512.h"
#include "script/script.h"
#include "script/standard.h"
#include "uint256.h"

int CCrypter::BytesToKeySHA512AES(const std::vector<unsigned char>& chSalt, const SecureString& strKeyData, int count, unsigned char* key, unsigned char* iv) const
{
    // This mimics the behavior of openssl's EVP_BytesToKey with an aes256cbc
    // cipher and sha512 message digest. Because sha512's output size (64b) is
    // greater than the aes256 block size (16b) + aes256 key size (32b),
    // there's no need to process more than once (D_0).

    if (!count || !key || !iv)
        return 0;

    unsigned char buf[CSHA512::OUTPUT_SIZE];
    CSHA512 di;

    di.Write((const unsigned char*)strKeyData.c_str(), strKeyData.size());
    di.Write(chSalt.data(), chSalt.size());
    di.Finalize(buf);

    for (int i = 0; i != count - 1; i++)
        di.Reset().Write(buf, sizeof(buf)).Finalize(buf);

    memcpy(key, buf, WALLET_CRYPTO_KEY_SIZE);
    memcpy(iv, buf + WALLET_CRYPTO_KEY_SIZE, WALLET_CRYPTO_IV_SIZE);
    memory_cleanse(buf, sizeof(buf));
    return WALLET_CRYPTO_KEY_SIZE;
}

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = BytesToKeySHA512AES(chSalt, strKeyData, nRounds, vchKey.data(), vchIV.data());

    if (i != (int)WALLET_CRYPTO_KEY_SIZE) {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_IV_SIZE)
        return false;

    memcpy(vchKey.data(), chNewKey.data(), chNewKey.size());
    memcpy(vchIV.data(), chNewIV.data(), chNewIV.size());

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char>& vchCiphertext) const
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCKSIZE bytes
    vchCiphertext.resize(vchPlaintext.size() + AES_BLOCKSIZE);

    AES256CBCEncrypt enc(vchKey.data(), vchIV.data(), true);
    size_t nLen = enc.Encrypt(&vchPlaintext[0], vchPlaintext.size(), vchCiphertext.data());
    if (nLen < vchPlaintext.size())
        return false;
    vchCiphertext.resize(nLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    vchPlaintext.resize(nLen);
    AES256CBCDecrypt dec(vchKey.data(), vchIV.data(), true);
    nLen = dec.Decrypt(vchCiphertext.data(), vchCiphertext.size(), &vchPlaintext[0]);
    if (nLen == 0)
        return false;
    vchPlaintext.resize(nLen);
    return true;
}

bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const uint256& nIV, std::vector<unsigned char>& vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    memcpy(chIV.data(), &nIV, WALLET_CRYPTO_IV_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt(*((const CKeyingMaterial*)&vchPlaintext), vchCiphertext);
}

bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_IV_SIZE);
    memcpy(chIV.data(), &nIV, WALLET_CRYPTO_IV_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}

bool CCryptoKeyStore::DecryptKey(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCryptedSecret, const CPubKey& vchPubKey, CKey& key)
{
    CKeyingMaterial vchSecret;
    if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
        return false;

    if (vchSecret.size() != 32)
        return false;

    key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
    return key.VerifyPubKey(vchPubKey);
}

bool CCryptoKeyStore::SetCrypted()
{
    LOCK(cs_KeyStore);
    if (fUseCrypto)
        return true;
    if (!mapKeys.empty() && !mapSaplingSpendingKeys.empty())
        return false;
    fUseCrypto = true;
    return true;
}

bool CCryptoKeyStore::IsLocked() const
{
    if (!IsCrypted()) {
        return false;
    }
    return WITH_LOCK(cs_KeyStore, return vMasterKey.empty());
}

bool CCryptoKeyStore::AddKeyPubKey(const CKey& key, const CPubKey& pubkey)
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::AddKeyPubKey(key, pubkey);
    }

    if (IsLocked()) {
        return false;
    }

    std::vector<unsigned char> vchCryptedSecret;
    CKeyingMaterial vchSecret(key.begin(), key.end());
    if (!EncryptSecret(vMasterKey, vchSecret, pubkey.GetHash(), vchCryptedSecret)) {
        return false;
    }

    if (!AddCryptedKey(pubkey, vchCryptedSecret)) {
        return false;
    }
    return true;
}

bool CCryptoKeyStore::HaveKey(const CKeyID& address) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::HaveKey(address);
    }
    return mapCryptedKeys.count(address) > 0;
}

bool CCryptoKeyStore::AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted()) {
        return false;
    }

    mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID& address, CKey& keyOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::GetKey(address, keyOut);
    }

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end()) {
        const CPubKey& vchPubKey = (*mi).second.first;
        const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
        return DecryptKey(vMasterKey, vchCryptedSecret, vchPubKey, keyOut);
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);
    }

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end()) {
        vchPubKeyOut = (*mi).second.first;
        return true;
    }
    // Check for watch-only pubkeys
    return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);
}

std::set<CKeyID> CCryptoKeyStore::GetKeys() const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::GetKeys();
    }
    std::set<CKeyID> set_address;
    for (const auto& mi : mapCryptedKeys) {
        set_address.insert(mi.first);
    }
    return set_address;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    LOCK(cs_KeyStore);
    if (!mapCryptedKeys.empty() || IsCrypted())
        return false;

    fUseCrypto = true;
    for (KeyMap::value_type& mKey : mapKeys) {
        const CKey& key = mKey.second;
        CPubKey vchPubKey = key.GetPubKey();
        CKeyingMaterial vchSecret(key.begin(), key.end());
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, vchSecret, vchPubKey.GetHash(), vchCryptedSecret))
            return false;
        if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
    }
    mapKeys.clear();
    return true;
}
