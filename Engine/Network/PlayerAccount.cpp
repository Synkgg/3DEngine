#include "PlayerAccount.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <algorithm>
#include <array>

namespace {
struct KeyStore {
    NCRYPT_PROV_HANDLE provider{};
    NCRYPT_KEY_HANDLE key{};
    ~KeyStore(){
        if(key) NCryptFreeObject(key);
        if(provider) NCryptFreeObject(provider);
    }
    bool open(){
        if(NCryptOpenStorageProvider(&provider,MS_KEY_STORAGE_PROVIDER,0)!=ERROR_SUCCESS) return false;
        constexpr auto name=L"DuelFPS player account v1";
        const auto status=NCryptOpenKey(provider,&key,name,0,NCRYPT_SILENT_FLAG);
        if(status==ERROR_SUCCESS) return true;
        if(status!=NTE_BAD_KEYSET) return false;
        const auto created=NCryptCreatePersistedKey(provider,&key,NCRYPT_ECDSA_P256_ALGORITHM,name,0,0);
        if(created==NTE_EXISTS) return NCryptOpenKey(provider,&key,name,0,NCRYPT_SILENT_FLAG)==ERROR_SUCCESS;
        return created==ERROR_SUCCESS && NCryptFinalizeKey(key,0)==ERROR_SUCCESS;
    }
};
std::string hex(const unsigned char* data,size_t size){
    constexpr char digits[]="0123456789abcdef";
    std::string result;result.reserve(size*2);
    for(size_t i=0;i<size;++i){result.push_back(digits[data[i]>>4]);result.push_back(digits[data[i]&15]);}
    return result;
}
}
#endif

std::string PlayerAccount::PublicKey(){
#ifdef _WIN32
    KeyStore store;
    if(!store.open()) return {};
    std::array<unsigned char,sizeof(BCRYPT_ECCKEY_BLOB)+64> blob{};
    DWORD count{};
    if(NCryptExportKey(store.key,0,BCRYPT_ECCPUBLIC_BLOB,nullptr,blob.data(),blob.size(),&count,0)!=ERROR_SUCCESS ||
       count!=blob.size()) return {};
    return "04"+hex(blob.data()+sizeof(BCRYPT_ECCKEY_BLOB),64);
#else
    return {};
#endif
}

std::string PlayerAccount::Sign(const std::string& action,const std::string& nonce){
#ifdef _WIN32
    if(action.empty() || action.size()>24 || action.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ.abcdefghijklmnopqrstuvwxyz")!=std::string::npos ||
       nonce.size()!=32 || nonce.find_first_not_of("abcdef0123456789")!=std::string::npos) return {};
    KeyStore store;
    if(!store.open()) return {};
    auto message="DuelFPS/v1:"+action+":"+nonce;
    std::array<unsigned char,32> digest{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(message.data()),
                  static_cast<ULONG>(message.size()),digest.data(),digest.size())!=0) return {};
    std::array<unsigned char,64> signature{};
    DWORD length{};
    if(NCryptSignHash(store.key,nullptr,digest.data(),digest.size(),signature.data(),signature.size(),&length,0)!=ERROR_SUCCESS ||
       length!=signature.size()) return {};
    return hex(signature.data(),length);
#else
    return {};
#endif
}
