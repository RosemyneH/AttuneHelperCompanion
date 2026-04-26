package com.attunehelper.companion;

public final class CredentialsStore {
    static {
        System.loadLibrary("credentials_ndk");
    }

    public static native boolean isStubAvailable();

    private CredentialsStore() {
    }
}
