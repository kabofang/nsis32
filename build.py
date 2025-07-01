import os
import shutil
import urllib.request
import zipfile
import argparse
import winreg
import subprocess

def download_and_extract(url, target_dir):
    print(f"Downloading {url}...")
    zip_path = os.path.basename(url)
    urllib.request.urlretrieve(url, zip_path)
    
    print(f"\nExtracting {zip_path} to {target_dir}...")
    
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        # The zip filename without extension IS the root directory name
        root_dir = os.path.splitext(zip_path)[0]  # "wxWidgets-3.2.8"
        
        # Extract all files (automatically preserves structure)
        zip_ref.extractall(target_dir)
    
    extracted_path = os.path.join(target_dir, target_dir)
    
    # Verify extraction
    print(f"\nExtraction complete. Root directory: {extracted_path}")
    if os.path.exists(extracted_path):
        print("Top-level contents:")
        for item in os.listdir(extracted_path)[:5]:
            print(f"- {item}")
    else:
        print("ERROR: Failed to extract to correct directory")

def run_command(command, cwd=None):
    print(f"\nExecuting in {cwd or os.getcwd()}: {command}")
    return os.system(f'cmd /c "{command}"')

def set_user_environment_variable(name, value):
    """
    Permanently sets a user environment variable in the registry.
    Only affects the current user (HKEY_CURRENT_USER).
    """
    try:
        with winreg.ConnectRegistry(None, winreg.HKEY_CURRENT_USER) as root:
            with winreg.OpenKey(root, "Environment", 0, winreg.KEY_ALL_ACCESS) as key:
                winreg.SetValueEx(key, name, 0, winreg.REG_EXPAND_SZ, value)
        return True
    except WindowsError as e:
        print(f"Failed to set environment variable: {e}")
        return False

def find_vs2022_installation():
    try:
        vswhere_path = os.path.join(
            os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
            "Microsoft Visual Studio", "Installer", "vswhere.exe"
        )
        
        result = subprocess.run(
            [vswhere_path, "-latest", "-products", "*", "-version", "17.0", "-property", "installationPath"],
            capture_output=True, text=True, check=True
        )
        
        if result.stdout.strip():
            return result.stdout.strip()
        return None
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

def find_vcvarsall():
    vs_path = find_vs2022_installation()
    if vs_path:
        vcvarsall_path = os.path.join(vs_path, "VC", "Auxiliary", "Build", "vcvarsall.bat")
        if os.path.exists(vcvarsall_path):
            return vcvarsall_path
    return None

def main():
    # Configure argument parser with named parameters
    parser = argparse.ArgumentParser(description="Build tool script")
    
    # Required download directory argument with both short and long forms
    parser.add_argument("--ddir",
                      dest="download_dir",
                      required=True,
                      help="Directory to store downloaded files (required)")
    
    # Optional Visual Studio path argument
    parser.add_argument("--vsdir",
                      dest="vs_dir",
                      default=None,
                      help="Path to Microsoft Visual Studio installation (optional)")
    
    # Architecture selection with restricted choices
    parser.add_argument("--arch",
                      choices=['x86', 'x64'],
                      default='x86',
                      help="Target architecture (x86 or x64, default: x86)")
    
    args = parser.parse_args()
    if not args.vs_dir:
        vcvarsall = find_vcvarsall()
        if not vcvarsall:
            print("Error: Could not find Visual Studio 2022 installation")
            return
    else:
        vcvarsall = os.path.join(args.vs_dir, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    print(vcvarsall)
    # Create download directory if it doesn't exist
    os.makedirs(args.download_dir, exist_ok=True)

    # 1. Handle wxWidgets
    wx_dir = os.path.join(args.download_dir, "wxWidgets")
    if not os.path.exists(wx_dir):
        wx_url = "https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.8/wxWidgets-3.2.8.zip"
        wx_dir = download_and_extract(wx_url, wx_dir)

        set_user_environment_variable("WXWIN", os.path.abspath(wx_dir))
        os.environ["WXWIN"] = os.path.abspath(wx_dir)
        print(f"\nWXWIN set to: {os.environ['WXWIN']}")

        setup_h_src = os.path.join(wx_dir, "include", "wx", "msw", "setup.h")
        setup_h_dest = os.path.abspath(os.path.join("Contrib", "NSIS Menu", "wx", "setup.h"))
        
        print(f"Copying setup.h from:\n{setup_h_src}\nto:\n{setup_h_dest}")
        os.makedirs(os.path.dirname(setup_h_dest), exist_ok=True)
        shutil.copy2(setup_h_src, setup_h_dest)

        wx_build_dir = os.path.abspath(os.path.join("Contrib", "NSIS Menu", "wx"))
        if os.path.exists(vcvarsall):
            command = f'"{vcvarsall}" {args.arch} && cd "{wx_build_dir}" && wxbuild.bat'
            run_command(command)
        else:
            print(f"\nERROR: vcvarsall.bat not found at {vcvarsall}")
            return

    # 2. Handle Zlib
    # https://nsis.sourceforge.io/Zlib
    if args.arch == 'x86':
        zlib_dir = os.path.join(args.download_dir, "Zlib32")
        zlib_url = "https://nsis.sourceforge.io/mediawiki/images/c/ca/Zlib-1.2.7-win32-x86.zip"
    else:
        zlib_dir = os.path.join(args.download_dir, "Zlib64")
        zlib_url = "https://nsis.sourceforge.io/mediawiki/images/b/bb/Zlib-1.2.8-win64-AMD64.zip"
    if not os.path.exists(zlib_dir):
        download_and_extract(zlib_url, zlib_dir)

    # 3. Run scons
    zlib_path = os.path.abspath(zlib_dir)
    scons_cmd = f'scons ZLIB_W32="{zlib_path}" NSIS_CONFIG_LOG=yes NSIS_MAX_STRLEN=8192 dist-zip'
    print(f"build command: {scons_cmd}")

if __name__ == "__main__":
    main()