import os
import subprocess
import sys
from pathlib import Path

import utils

from io import BytesIO
from urllib.request import urlopen
from zipfile import ZipFile

VULKAN_SDK = os.environ.get('VULKAN_SDK')
VULKAN_SDK_INSTALLER_URL = 'https://sdk.lunarg.com/sdk/download/1.2.170.0/windows/vulkan_sdk.exe'
WHIP_VULKAN_VERSION = '1.2.170.0'
VULKAN_SDK_EXE_PATH = 'Whip/vendor/VulkanSDK/VulkanSDK.exe'

def install_vulkanSDK():
    print('Downloading {} to {}'.format(VULKAN_SDK_INSTALLER_URL, VULKAN_SDK_EXE_PATH))
    utils.download_file(VULKAN_SDK_INSTALLER_URL, VULKAN_SDK_EXE_PATH)
    print("Done!")
    print("Running Vulkan SDK installer...")
    os.startfile(os.path.abspath(VULKAN_SDK_EXE_PATH))
    print("Re-run this script after installation")

def install_vulkan_prompt():
    print("Would you like to install the Vulkan SDK?")
    install = utils.yes_or_no()
    if (install):
        install_vulkanSDK()
        quit()

def check_vulkanSDK():
    if (VULKAN_SDK is None):
        print("You don't have the Vulkan SDK installed!")
        install_vulkan_prompt()
        return False
    elif (WHIP_VULKAN_VERSION not in VULKAN_SDK):
        print(f"Located Vulkan SDK at {VULKAN_SDK}")
        print(f"You don't have the correct Vulkan SDK version! (Hazel requires {WHIP_VULKAN_VERSION})")
        install_vulkan_prompt()
        return False

    print(f"Correct Vulkan SDK located at {VULKAN_SDK}")
    return True

VulkanSDKDebugLibsURL = 'https://files.lunarg.com/SDK-1.2.170.0/VulkanSDK-1.2.170.0-DebugLibs.zip'
OutputDirectory = "Hazel/vendor/VulkanSDK"
TempZipFile = f"{OutputDirectory}/VulkanSDK.zip"

def check_vulkanSDK_debug_libs():
    shadercdLib = Path(f"{OutputDirectory}/Lib/shaderc_sharedd.lib")
    if (not shadercdLib.exists()):
        print(f"No Vulkan SDK debug libs found. (Checked {shadercdLib})")
        print("Downloading", VulkanSDKDebugLibsURL)
        with urlopen(VulkanSDKDebugLibsURL) as zipresp:
            with ZipFile(BytesIO(zipresp.read())) as zfile:
                zfile.extractall(OutputDirectory)

    print(f"Vulkan SDK debug libs located at {OutputDirectory}")
    return True