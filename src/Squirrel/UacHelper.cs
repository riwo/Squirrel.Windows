using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Principal;
using Microsoft.Win32;

namespace Squirrel {
    public static class UacHelper {
        private static uint STANDARD_RIGHTS_READ = 0x00020000;
        private static uint TOKEN_QUERY = 0x0008;
        private static uint TOKEN_READ = (STANDARD_RIGHTS_READ | TOKEN_QUERY);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool OpenProcessToken(IntPtr ProcessHandle, UInt32 DesiredAccess, out IntPtr TokenHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool CloseHandle(IntPtr hObject);

        [DllImport("advapi32.dll", SetLastError = true)]
        public static extern bool GetTokenInformation(IntPtr TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, IntPtr TokenInformation, uint TokenInformationLength, out uint ReturnLength);

        public enum TOKEN_INFORMATION_CLASS {
            TokenUser = 1,
            TokenGroups,
            TokenPrivileges,
            TokenOwner,
            TokenPrimaryGroup,
            TokenDefaultDacl,
            TokenSource,
            TokenType,
            TokenImpersonationLevel,
            TokenStatistics,
            TokenRestrictedSids,
            TokenSessionId,
            TokenGroupsAndPrivileges,
            TokenSessionReference,
            TokenSandBoxInert,
            TokenAuditPolicy,
            TokenOrigin,
            TokenElevationType,
            TokenLinkedToken,
            TokenElevation,
            TokenHasRestrictions,
            TokenAccessInformation,
            TokenVirtualizationAllowed,
            TokenVirtualizationEnabled,
            TokenIntegrityLevel,
            TokenUIAccess,
            TokenMandatoryPolicy,
            TokenLogonSid,
            MaxTokenInfoClass
        }

        public enum TOKEN_ELEVATION_TYPE {
            TokenElevationTypeDefault = 1,
            TokenElevationTypeFull,
            TokenElevationTypeLimited
        }

        public static bool IsUacEnabled {
            get {
                using (var uacKey = Registry.LocalMachine.OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", false)) {
                    return uacKey.GetValue("EnableLUA").Equals(1);
                }
            }
        }

        public static bool IsProcessElevated {
            get {
                using (var identity = WindowsIdentity.GetCurrent()) {
                    if (identity.IsSystem)
                        return true;
                }

                if (IsUacEnabled) {
                    if (!OpenProcessToken(Process.GetCurrentProcess().Handle, TOKEN_READ, out var tokenHandle))
                        throw new ApplicationException("Could not get process token.  Win32 Error Code: " + Marshal.GetLastWin32Error());

                    try
                    {
                        var elevationResultSize = Marshal.SizeOf(typeof(TOKEN_ELEVATION_TYPE).GetEnumUnderlyingType());
                        var elevationTypePtr = Marshal.AllocHGlobal(elevationResultSize);
                        try
                        {
                            var success = GetTokenInformation(tokenHandle, TOKEN_INFORMATION_CLASS.TokenElevationType,
                                                              elevationTypePtr, (uint)elevationResultSize,
                                                              out _);
                            if (!success)
                                throw new ApplicationException("Unable to determine the current elevation.");

                            var elevationResult = (TOKEN_ELEVATION_TYPE)Marshal.ReadInt32(elevationTypePtr);
                            return elevationResult == TOKEN_ELEVATION_TYPE.TokenElevationTypeFull;
                        }
                        finally {
                            if (elevationTypePtr != IntPtr.Zero)
                                Marshal.FreeHGlobal(elevationTypePtr);
                        }
                    }
                    finally {
                        if (tokenHandle != IntPtr.Zero)
                            CloseHandle(tokenHandle);
                    }
                } else {
                    var identity = WindowsIdentity.GetCurrent();
                    var principal = new WindowsPrincipal(identity);
                    return principal.IsInRole(WindowsBuiltInRole.Administrator) || principal.IsInRole(0x200); //Domain Administrator
                }
            }
        }

        public static RegistryKey GetRegistryBaseKey() {
            return IsProcessElevated ? 
                RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32) :
                RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, RegistryView.Default);
        }
    }
}