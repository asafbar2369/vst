#include "SecureStorage.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <wincrypt.h>
 #pragma comment (lib, "crypt32.lib")
#else
 #include <sys/stat.h>
#endif

namespace vsx
{
SecureStorage::SecureStorage (juce::File folder) : dir (folder)
{
    if (dir == juce::File())
        dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VICE SYNTH X").getChildFile ("Credentials");
    dir.createDirectory();
}

bool SecureStorage::isStrongEncryptionAvailable()
{
   #if JUCE_WINDOWS
    return true;
   #else
    return false;
   #endif
}

juce::String SecureStorage::mask (const juce::String& key)
{
    if (key.length() <= 8) return juce::String::repeatedString ("*", key.length());
    return key.substring (0, 4) + juce::String::repeatedString ("*", 8) + key.substring (key.length() - 4);
}

juce::File SecureStorage::fileFor (const juce::String& providerId) const
{
    return dir.getChildFile (juce::File::createLegalFileName (providerId) + ".key");
}

juce::MemoryBlock SecureStorage::protect (const juce::MemoryBlock& plain)
{
   #if JUCE_WINDOWS
    DATA_BLOB in { (DWORD) plain.getSize(), (BYTE*) plain.getData() };
    DATA_BLOB out {};
    if (CryptProtectData (&in, L"VICE SYNTH X AI key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        juce::MemoryBlock result (out.pbData, out.cbData);
        LocalFree (out.pbData);
        return result;
    }
    return {};
   #else
    // Obfuscation only (see class documentation).
    juce::MemoryBlock result (plain);
    auto* d = static_cast<juce::uint8*> (result.getData());
    const char* salt = "VSX-obfuscation-not-encryption";
    for (size_t i = 0; i < result.getSize(); ++i) d[i] ^= (juce::uint8) salt[i % 30];
    return result;
   #endif
}

juce::MemoryBlock SecureStorage::unprotect (const juce::MemoryBlock& blob)
{
   #if JUCE_WINDOWS
    DATA_BLOB in { (DWORD) blob.getSize(), (BYTE*) blob.getData() };
    DATA_BLOB out {};
    if (CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        juce::MemoryBlock result (out.pbData, out.cbData);
        SecureZeroMemory (out.pbData, out.cbData);
        LocalFree (out.pbData);
        return result;
    }
    return {};
   #else
    return protect (blob); // XOR is symmetric
   #endif
}

bool SecureStorage::storeKey (const juce::String& providerId, const juce::String& key)
{
    const auto f = fileFor (providerId);
    if (key.isEmpty())
    {
        f.deleteFile();
        return true;
    }
    juce::MemoryBlock plain (key.toRawUTF8(), key.getNumBytesAsUTF8());
    const auto blob = protect (plain);
    plain.fillWith (0);
    if (blob.isEmpty()) return false;
    if (! f.replaceWithData (blob.getData(), blob.getSize())) return false;
   #if ! JUCE_WINDOWS
    chmod (f.getFullPathName().toRawUTF8(), S_IRUSR | S_IWUSR);
   #endif
    return true;
}

juce::String SecureStorage::loadKey (const juce::String& providerId) const
{
    const auto f = fileFor (providerId);
    juce::MemoryBlock blob;
    if (! f.existsAsFile() || ! f.loadFileAsData (blob) || blob.isEmpty()) return {};
    auto plain = unprotect (blob);
    juce::String key = juce::String::fromUTF8 (static_cast<const char*> (plain.getData()), (int) plain.getSize());
    plain.fillWith (0);
    return key;
}

void SecureStorage::removeKey (const juce::String& providerId)
{
    fileFor (providerId).deleteFile();
}

} // namespace vsx
