Corstone SSE-300 with Ethos-U55 Example Subsystem for MPS3 (AN547)
==================================================================

Building TF-M
-------------

Follow the instructions in :doc:`Building instructions </building/tfm_build_instruction>`.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. note::

   These builds can be run on the Corstone-300 Ethos-U55 Ecosystem FVP
   (FVP_SSE300_MPS3) from version 11.22 or higher.

.. note::

   Provisioning bundles can be generated with the ``-DPLATFORM_DEFAULT_PROVISIONING=OFF``
   flag. The provisioning bundle binary will be generated and it's going to contain the
   provisioning code and provisioning values.

.. note::

   If ``-DPLATFORM_DEFAULT_PROVISIONING=OFF`` and ``-DTFM_DUMMY_PROVISIONING=OFF`` are set
   then unique values can be used for provisioning. The ${MCUBOOT_KEY_S} and ${MCUBOOT_KEY_NS}
   will be used for signing and provisioning so any unique private signing key can be used.
   If ``-DMCUBOOT_GENERATE_SIGNING_KEYPAIR=ON`` is set then a new mcuboot signing public and private
   keypair is going to be generated and it's going to be used to sign the S and NS binaries.

   The hash of the public key is going to be written into the ``provisioning_data.c`` automatically.
   The other keys and seeds can be changed by passing the new values to the build command,
   otherwise the default values going to be used:
   ``tf-m/platform/ext/target/arm/mps3/common/provisioning/provisioning_config.cmake``
   Optionally it's possible to pass a new config file with the ``-DPROVISIONING_KEYS_CONFIG``
   flag.

   The new generated keypair can be found in the ``<build dir>/bin`` folder or in the
   ``<install directory>/image_signing/keys`` after installation.
   The generated provisioning_data.c file can be found at
   ``<build directory>/platform/target/provisioning/provisioning_data.c``

   The provisioning bundle generation depends on pyelftools that's have to be installed::

    pip3 install pyelftools

Build instructions with platform name: arm/mps3/corstone300/an547
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-DTFM_PLATFORM=arm/mps3/corstone300/an547``

.. note::

   This platform support does not provide software for Ethos-U55 IP, only
   contains base address and interrupt number for it.

.. note::

   The built binaries can be run on the Corstone SSE-300 with Ethos-U55
   Example Subsystem for MPS3 (AN547).
   They can also be run on the Corstone-300 Ethos-U55 Ecosystem FVP
   (FVP_SSE300_MPS3) up until version 11.15. From version 11.16, the FVP
   is aligned with the AN552 FPGA platform.

Build instructions with platform name: arm/mps3/corstone300/an552
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-DTFM_PLATFORM=arm/mps3/corstone300/an552``

.. note::

   This platform support does not provide software for Ethos-U55 IP, only
   contains base address and interrupt number for it.

Build instructions with platform name: arm/mps3/corstone300/fvp
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``-DTFM_PLATFORM=arm/mps3/corstone300/fvp``

.. note::

   This platform support does not provide software for Ethos-U55 IP, only
   contains base address and interrupt number for it.

To run the example code on Corstone SSE-300 with Ethos-U55 Example Subsystem for MPS3 (AN547)
---------------------------------------------------------------------------------------------
FPGA image is available to download `here <https://developer.arm.com/tools-and-software/development-boards/fpga-prototyping-boards/download-fpga-images>`__

To run BL2 bootloader and TF-M example application and tests in the MPS3 board,
it is required to have AN547 image in the MPS3 board SD card. The image should
be located in ``<MPS3 device name>/MB/HBI<BoardNumberBoardrevision>/AN547``

The MPS3 board tested is HBI0309C.

#. Copy ``bl2.bin`` and ``tfm_s_ns_signed.bin`` files from
   build dir to ``<MPS3 device name>/SOFTWARE/``
#. Rename ``tfm_s_ns_signed.bin`` to ``tfm.bin`` (Filename should not be longer
   than 8 charachters.)
#. Open ``<MPS3 device name>/MB/HBI0309C/AN547/images.txt``
#. Update the ``images.txt`` file as follows::

    TITLE: Arm MPS3 FPGA prototyping board Images Configuration File

    [IMAGES]
    TOTALIMAGES: 2                     ;Number of Images (Max: 32)

    IMAGE0UPDATE: AUTO                 ;Image Update:NONE/AUTO/FORCE
    IMAGE0ADDRESS: 0x00000000          ;Please select the required executable program
    IMAGE0FILE: \SOFTWARE\bl2.bin
    IMAGE1UPDATE: FORCEQSPI
    IMAGE1ADDRESS: 0x00000000
    IMAGE1FILE: \SOFTWARE\tfm.bin

#. Close ``<MPS3 device name>/MB/HBI0309C/AN547/images.txt``
#. Unmount/eject the ``<MPS3 device name>`` unit
#. Reset the board to execute the TF-M example application
#. After completing the procedure you should be able to visualize on the serial
   port (baud 115200 8n1) the following messages::

    [INF] Starting bootloader
    [INF] [INF] Beginning provisioning

    [WRN] TFM_DUMMY_PROVISIONING is not suitable for production! This device is NOT SECURE
    [INF] [INF] Waiting for provisioning bundle

    [INF] [INF] Running provisioning bundle

    [INF] Swap type: none
    [INF] Swap type: none
    [INF] Bootloader chainload address offset: 0x0
    [INF] Jumping to the first image slot
    [Sec Thread] Secure image initializing!
    TF-M isolation level is: 0x00000001
    Booting TF-M v1.8.0

.. note::

   Some of the messages above are only visible when ``CMAKE_BUILD_TYPE`` is set
   to ``Debug``.

.. note::

   If ``-DPLATFORM_DEFAULT_PROVISIONING=OFF`` is set then the provisioning bundle has to
   be placed on the ``0x10022400`` address by copying ``encrypted_provisioning_bundle.bin`` and
   renaming it to ``prv.bin``, then extending the images.txt with::

    IMAGE2UPDATE: AUTO
    IMAGE2ADDRESS: 0x00022400
    IMAGE2FILE: \SOFTWARE\prv.bin

To run the example code on Corstone SSE-300 with Ethos-U55 Example Subsystem for MPS3 (AN552)
---------------------------------------------------------------------------------------------
FPGA image is available to download `here <https://developer.arm.com/tools-and-software/development-boards/fpga-prototyping-boards/download-fpga-images>`__

To run BL2 bootloader and TF-M example application and tests in the MPS3 board,
it is required to have AN552 image in the MPS3 board SD card. The image should
be located in ``<MPS3 device name>/MB/HBI<BoardNumberBoardrevision>/AN552``

The MPS3 board tested is HBI0309C.

#. Copy ``bl2.bin`` and ``tfm_s_ns_signed.bin`` files from
   build dir to ``<MPS3 device name>/SOFTWARE/``
#. Rename ``tfm_s_ns_signed.bin`` to ``tfm.bin`` (Filename should not be longer
   than 8 charachters.)
#. Open ``<MPS3 device name>/MB/HBI0309C/AN552/images.txt``
#. Update the ``images.txt`` file as follows::

    TITLE: Arm MPS3 FPGA prototyping board Images Configuration File

    [IMAGES]
    TOTALIMAGES: 2                     ;Number of Images (Max: 32)

    IMAGE0UPDATE: AUTO                 ;Image Update:NONE/AUTO/FORCE
    IMAGE0ADDRESS: 0x00000000          ;Please select the required executable program
    IMAGE0FILE: \SOFTWARE\bl2.bin
    IMAGE1UPDATE: FORCEQSPI
    IMAGE1ADDRESS: 0x00000000
    IMAGE1FILE: \SOFTWARE\tfm.bin

#. Close ``<MPS3 device name>/MB/HBI0309C/AN552/images.txt``
#. Unmount/eject the ``<MPS3 device name>`` unit
#. Reset the board to execute the TF-M example application
#. After completing the procedure you should be able to visualize on the serial
   port (baud 115200 8n1) the following messages::

    [INF] Starting bootloader
    [INF] [INF] Beginning provisioning

    [WRN] TFM_DUMMY_PROVISIONING is not suitable for production! This device is NOT SECURE
    [INF] [INF] Waiting for provisioning bundle

    [INF] [INF] Running provisioning bundle

    [INF] Swap type: none
    [INF] Swap type: none
    [INF] Bootloader chainload address offset: 0x0
    [INF] Jumping to the first image slot
    [Sec Thread] Secure image initializing!
    TF-M isolation level is: 0x00000001
    Booting TF-M v1.8.0

.. note::

   Some of the messages above are only visible when ``CMAKE_BUILD_TYPE`` is set
   to ``Debug``.

.. note::

   If ``-DPLATFORM_DEFAULT_PROVISIONING=OFF`` is set then the provisioning bundle has to
   be placed on the ``0x10022400`` address by copying ``encrypted_provisioning_bundle.bin`` and
   renaming it to ``prv.bin``, then extending the images.txt with::

    IMAGE2UPDATE: AUTO
    IMAGE2ADDRESS: 0x00022400
    IMAGE2FILE: \SOFTWARE\prv.bin

To run the example code on Corstone-300 Ethos-U55 Ecosystem FVP
---------------------------------------------------------------
FVP is available to download `here <https://developer.arm.com/tools-and-software/open-source-software/arm-platforms-software/arm-ecosystem-fvps>`__

#. Install the FVP
#. Copy ``bl2.axf`` and ``tfm_s_ns_signed.bin`` files from
   build dir to ``<FVP installation path>/models/Linux64_GCC-6.4/``
#. Navigate to the same directory and execute the following command to start FVP::

    $ ./FVP_Corstone_SSE-300_Ethos-U55 -a cpu0*="bl2.axf" --data "tfm_s_ns_signed.bin"@0x38000000

#. After completing the procedure you should be able to visualize on the serial
   port the following messages::

    Trying 127.0.0.1...
    Connected to localhost.
    Escape character is '^]'.
    [INF] Starting bootloader
    [INF] [INF] Beginning provisioning

    [WRN] TFM_DUMMY_PROVISIONING is not suitable for production! This device is NOT SECURE
    [INF] [INF] Waiting for provisioning bundle

    [INF] [INF] Running provisioning bundle

    [INF] Swap type: none
    [INF] Swap type: none
    [INF] Bootloader chainload address offset: 0x0
    [INF] Jumping to the first image slot
    [Sec Thread] Secure image initializing!
    TF-M isolation level is: 0x00000001
    Booting TF-M v1.8.0

.. note::

   Some of the messages above are only visible when ``CMAKE_BUILD_TYPE`` is set
   to ``Debug``.

.. note::

   If ``-DPLATFORM_DEFAULT_PROVISIONING=OFF`` is set then the provisioning bundle has to
   be placed on the ``0x10022000`` address with::

   $ ./FVP_Corstone_SSE-300_Ethos-U55 -a cpu0*="bl2.axf" --data "tfm_s_ns_signed.bin"@0x38000000 --data "encrypted_provisioning_bundle.bin"@0x10022000


-------------

*Copyright (c) 2020-2023, Arm Limited. All rights reserved.*
