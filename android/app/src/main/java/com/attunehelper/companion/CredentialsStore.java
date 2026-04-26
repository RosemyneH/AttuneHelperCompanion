package com.attunehelper.companion;

public final class CredentialsStore {
    private static final boolean NATIVE_LIB_LOADED;

    static {
        boolean ok = false;
        try {
            System.loadLibrary("credentials_ndk");
            ok = true;
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        NATIVE_LIB_LOADED = ok;
    }

    public static boolean isStubAvailable() {
        if (!NATIVE_LIB_LOADED) {
            return false;
        }
        return isStubAvailableNative();
    }

    private static native boolean isStubAvailableNative();

    private CredentialsStore() {
    }
}
