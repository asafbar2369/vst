#pragma once

#include <juce_core/juce_core.h>

namespace vsx
{
/**
    Secure storage of AI provider credentials.

    Windows: Data Protection API (CryptProtectData, current-user scope) - the encrypted blob is
    only decryptable by the same Windows user account.
    Other platforms (development builds only): the key file is written with owner-only (0600)
    permissions and lightly obfuscated; this is NOT strong encryption and is documented as such.

    Keys are never written into presets, plug-in state, logs or the conversation history.
*/
class SecureStorage
{
public:
    explicit SecureStorage (juce::File folder = {});

    bool storeKey (const juce::String& providerId, const juce::String& key);
    juce::String loadKey (const juce::String& providerId) const;
    bool hasKey (const juce::String& providerId) const { return loadKey (providerId).isNotEmpty(); }
    void removeKey (const juce::String& providerId);

    static bool isStrongEncryptionAvailable();
    static juce::String mask (const juce::String& key);

private:
    juce::File fileFor (const juce::String& providerId) const;
    static juce::MemoryBlock protect (const juce::MemoryBlock& plain);
    static juce::MemoryBlock unprotect (const juce::MemoryBlock& blob);
    juce::File dir;
};

} // namespace vsx
