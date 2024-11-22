[TOC]

# Bootloader Features
The bootloader allows for:
  - Starting the programmed application
  - Programming new applications
  - Resetting the Object Dictionary
  - Setting module node number

> [!NOTE]
> The bootloader does not implement an Object Dictionary, and only responds to specific bootloader commands.

# Configuring Bootloader Builds
The bootloader is configured via the ```config.h``` file located within Core/Inc. The only parameter in this file that should be changed is ```WRP_ENABLE```, which enables write protection over the bootloader on first application launch. Prebuilt binaries of the bootloader will have this flag enabled. Modifying other paramters in this file can cause unexpected behavior within applications. Any applications run via the bootloader must have a matching memory configuration (Application Size, Parameter Location, EEPROM Location and Size). The [Generic Module](https://github.com/COSMIIC-Inc/Implantables-GenericModule-Application) is compatible with the standard bootloader.

## Assigning Serial Number and Module Type
The Serial Number and Module Type Idenfitier live in the last three bytes of the assigned bootloader space in flash (Addresses ```0x08004FFD``` - ```0x08004FFF```). To prevent unexpected behavior, these parameters should be written to the end of the bootloader ```.bin``` image before it gets programmed to the bootloader at address ```0x08000000```.

The following Matlab script can be used to program these parameters.

<details>
  <summary>ST_AssignSN.m</summary>
  ```
  %% Set Parameters
  SerialNumber = 1234;
  moduleType = 4;

  %% Get File Handle and Size
  [bin, location] = uigetfile('*.bin');
  fid = fopen([location bin]);
  fseek(fid, 0, 'eof');
  filesize = ftell(fid);
  fclose(fid);
  if(filesize>20477)
      error("File is too large to assign serial number and module type")
  end
  %% Get File Data
  fid = fopen([location bin]);
  dataWrite = zeros(20480, 1);

  SerialNumberConvert = typecast(uint16(SerialNumber), 'uint8');

  if(moduleType > 255)
      error("Invalid moduleType")
  end

  dataGen = fread (fid, filesize);

  dataWrite(1:filesize) = dataGen;
  fclose(fid);

  %% Write bin data to dataWrite
  dataWrite(20478) = SerialNumberConvert(2);
  dataWrite(20479) = SerialNumberConvert(1);
  dataWrite(20480) = moduleType;

  %% Write modified bin to disk
  bin = [bin(1:end-4) '_SN' num2str(SerialNumber) '.bin'];
  fid = fopen([location bin], 'w');
  fwrite(fid, dataWrite);

  fclose(fid);
  sprintf('Write file %s with Serial Number %d and moduleType %d', bin, SerialNumber, moduleType)
  ```
</details>


# Configuring Application Builds
Because the bootloader lives in the first 20K of flash, the flashed application must have changes made to several files in order to ensure correct operation.
If you are creating an application based on the [Generic Module](https://github.com/COSMIIC-Inc/Implantables-GenericModule-Application), these changes are already in place.

## Core/Src/system_stm32l4xx.c (Application)
The following changes are made to the @ref system_stm32l4xx.c file:
- ```#define USER_VECT_TAB_ADDRESS``` - is defined
- ```#define VECT_TAB_OFFSET 0x00005000U``` - changed from 0x00000000U

## STM32L433RCTXP_FLASH.ld (Application)
This is the linker script that defines the addressing of FLASH, RAM, and other important addresses of the microcontroller.

The only change is made to the *Memories definiton* section.

```c
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 48K
  RAM2    (xrw)    : ORIGIN = 0x10000000,   LENGTH = 16K
  FLASH    (rx)    : ORIGIN = 0x08005000,   LENGTH = 232K
}
```

The flash origin has a 0x5000 offset due to the bootloader, and the length is shortened, due to the bootloader, module parameter storage, and reserved EEPROM space.

For a 256K module:
- 256K - 20K (bootloader) - 2K (parameter space) - 2K (EEPROM) = 232K

# Using the Bootloader
In Matlab, call ```rmbootloader(nnp)``` to open the remote module bootloader program. Clicking 'Scan for Nodes' will probe the network for available nodes.

![](/manual/images/BootMain.png "rmbootloader Main Screen")

![](/manual/images/NodeScan.png "Node Polling Modal")

![](/manual/images/BootNodeList.png "Node Populated List")

Selecting a node in the list and clicking 'Select Node' will select the module in the bootloader and populate node information.

![](/manual/images/BootSelectNodeEBSA.png "Selected Node")

From here, you can load a new application, or start the app.

In the __Exit Bootloader/Start App__ section, there are two buttons. The left button 'Selected Node' will only start the application on the currently selected node. The checkbox above it, 'Restore OD Defaults' will reset the Object Dictionary of the selected device before it starts (full erase of the emulated EEPROM) if selected. The 'All Nodes' button will start all nodes on the network, without any changes to their Object Dictionaries. 

![](/manual/images/BootSelectNodeRMF.png)

The __Remote Module Flash__ section is where you can load a new program onto a module. The checkboxes identify which parts of the programming sequence will take place. To overwrite a new program over an existing one, the flash must erased before a new program can be loaded. The functions of each of the programming sequences are below:

- **Erase:** Erases the application space of the module
- **BlankCheck:** Ensures that the application space is fully blank, if not returns the address of the first non-blank result.
- **Write:** Writes the new application to flash
- **Verify:** Read back the application flash to ensure all bytes are written correctly, if not, returns the address of the first conflict.

Clicking start will open a file select dialog to select a firmware ```.bin``` file to upload to the module.

Several progress bars indicating the progress of each stage will appear. If the process completes successfully, a modal similar to that below should appear:

![](/manual/images/FinalUploadScreen.png)

## Troubleshooting
| Issue | Resolution |
| ----- | ---------- |
| No network activity | - Ensure that all modules connected to the network are properly configured to operate on the network. Floating inputs will cause inoperation of the network. | 
| The bootloader says "Node is Running Application. Power cycle network to enter bootloader mode" | - Node is Running Application. Power cycle network to enter bootloader mode.\n - If restarting via CLI, use ```nnp.networkOnBootloader```, not ```nnp.networkOn``` |
| My module doesn't appear in the network | - Ensure network cables are connected correctly (BUSA -> BUSA, BUSB -> BUSB).\n - Ensure module core is not in a lockup mode. |
| Blankcheck found non-erased byte after erasing | - Attempt to erase the module again.\n - Restart the network. |
| Writing to the module randomly fails during upload | - Ensure the Power Module has a good radio connection to the access point. |
| Verify failed | - Ensure that the uploaded file matches the file being verified.\n - Restart the network. |
| "Node may not be on the network. Run a scan to see available nodes" | - Ensure network cables are connected correctly.\n - Ensure the Power Module has a good radio connection to the access point. |
| "Could not confirm Network Status. Toggle Network button to update." | - Toggle network button, if that doesn't work, power cycle the access point and reopen rmbootloader. |

# Polling Module Information In Application
The bootloader space contains the Serial Number, and Module Type "Identifier" in the last three bytes of its defined space. Their default locations are as follows:
 - 0x08004FFD - Serial Number High Byte
 - 0x08004FFE - Serial Number Low Byte
 - 0x08004FFF - Module Type Identifier

Other module parameters are located in the last page of flash memory. (For the STM32L433, these are located at address ```127*FLASH_PAGE_SIZE```). The order of parameters are:
 - UNS8 - Node Number
 - UNS8 - App Checksum
 - UNS24 - App Size
 - UNS8 - Checksum Clock (Unused)
 
> [!NOTE]
> The generic module application will automatically pull information from their locations and assign them in the Object Dictionary