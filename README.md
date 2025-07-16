# EPICS Driver for PicoScope 6000E Series

## Overview
This document provides detailed information about the EPICS driver for PicoTech's [Picoscope 6000E Series](https://www.picotech.com/oscilloscope/picoscope-6000e-series-3-ghz-5gss-deep-memory-mso-usb-oscilloscope) oscilloscope. The driver allows control and monitoring of the oscilloscope through EPICS Process Variables (PVs).

>[!Important] 
>Before running the application on a new IOC host, run `sudo ./udev_install.sh` to add required udev rule. 
>
> This script installs a udev rule to allow non-root access to the PicoScope USB device.

## Project Layout

```bash
DRIVER_Picoscope6000ESeries
├── README.md                    # This documentation
├── udev_install.sh              # Adds udev rules for PicoScope USB

├── PicoscopeApp/
│   ├── Db/                      # Database and template files (Picoscope.db, Picoscope.template)
│   ├── picoscopeSupport/
│   │   ├── include/libps6000a/  # Copy Pico SDK headers here
│   │   └── lib/                 # Copy Pico SDK shared libraries here
│   └── src/                     # All driver source files (.c/.cpp/.h)

├── iocBoot/
│   └── ioc<hostname>/           # st.cmd and IOC boot files
│       └── st.cmd/              # st.cmd start script

```
## Requirements
- EPICS Base (tested with 7.0.7 and 7.0.9). Make sure to update the `EPICS_BASE` path in `configure/RELEASE` to your local EPICS installation.
- GNU Make, g++ (Linux x86_64)
- PicoScope SDK for Linux (See below)

## Setup

> **Supported Platform:** Linux (x86_64 only)

### 1. Download the SDK
> ⚠️ This project requires SDK libraries from Pico Technology Ltd., which are **not GPL-licensed**.
> Please install them via the Pico Technology website. See the License section below for details.

Download and install the **PicoScope 7 for Linux** package from the [PicoScope 7 for Linux package](https://www.picotech.com/downloads/linux).  
This will install the required SDK (`libps6000a`) under `/opt/picoscope`.

### 2. Copy Required Files

After installing the SDK, copy the following files:

- **Header Files:**  
  From:  
  `/opt/picoscope/include/libps6000a/`  
  To:  
  `DRIVER_Picoscope6000ESeries/PicoscopeApp/picoscopeSupport/include/libps6000a/`

- **Shared Libraries:**  
  From:  
  `/opt/picoscope/lib/`  
  - Files:
    - `libps6000a.so`  
    - `libps6000a.so.2`  
    - `libps6000a.so.2.0.0`  
  
  To:  
  `DRIVER_Picoscope6000ESeries/PicoscopeApp/picoscopeSupport/lib/`

### 3. Update IOC Folder and Startup Script

To prepare the IOC for your system:

- **Rename the IOC directory**  
  Rename the folder `iocBoot/iocXXXX-XXX/` to match your actual IOC host name.  
  For example, if your host is `IOC0000-000`, rename the directory:
  ```bash
  mv iocBoot/iocXXXX-XXX/ iocBoot/iocIOC0000-000/
  ```

- **Edit the IOC startup script**  
  Open the `st.cmd` file inside the renamed IOC folder and update the following macros:

  - Replace `OSC=OSCXXXX-XX` with your desired PV name prefix (e.g., `OSC1234-01`)
  - Replace `SERIAL_NUM=XXXX/XXXX` with the actual serial number of your PicoScope (e.g., `JR000/1234`)

  Example line after editing:
  ```tcl
  dbLoadRecords("PicoscopeApp/Db/Picoscope.db", "OSC=OSC1234-01,SERIAL_NUM=JR000/1234")
  ```

  > 💡 The `OSC` prefix determines the root of your PV names (e.g., `OSC1234-01:ON`, `OSC1234-01:CHB:waveform`).  
  > The `SERIAL_NUM` must exactly match the value printed on the back 


## Usage
- The driver connects to (opens) the Picoscope identified by the serial number supplied in the `SERIAL_NUM` macro defined in the `st.cmd` startup script. This must match the serial number of the connected device exactly.
- It is expected that a PicoScope is connected at application start-up. If a PicoScope is connected after the application has started, setting `<OSCNAME>:ON` to `1` or `ON` will open the scope.  
- **Simple usage example walkthrough:**

  The following example demonstrates capturing a waveform on Channel B, using a ±20 V range.

  ```bash
   cd DRIVER_Picoscope6000ESeries/
   make clean all
   cd iocBoot/iocIOC0000-000/
   ./st.cmd

   # On another terminal
   caput OSC1234-01:CHB:range 20V         # Set the voltage range for Channel B to +/- 20V
   caget OSC1234-01:CHB:ON:fbk            # Check if Channel B is ON. If not, try:  caput OSC1234-01:CHB:ON ON.
   caput OSC1234-01:CHB:waveform:start 1  # Begin waveform acquisition (external trigger assumed)
   caget OSC1234-01:CHB:waveform          # Once waveform is ready, get the waveform.
  ```
  *See below for complete details on [device](#device-configurations), [channel](#channel-configurations), [data capture](#data-capture-configurations) configuration options.*

  **The waveform data is a scaled value. The calculation is located at the bottom.**
  - This will retrieve the waveform using the latest values of the data capture configuration PVs.    
  - To acquire a waveform for a specific channel, the PV `<OSCNAME>:CH[A-D]:ON` must be set to ON. Requesting `OSC1234-01:waveform:start` will fail if `OSC1234-01:ON` is set to OFF. 
  - The waveform data will be returned in the PV `<OSCNAME>:CH[A-D]:waveform`. 

>**Note:** 
>Data capture configuration PVs have :fbk PVs. These are updated with a put to `OSCNAME:waveform:start`. The value of the :fbk PVs contain the settings used to capture the LAST waveform.

## PV Index Table
### [Device Control](#device-control-1)
| PV | Description |
|----|-------------|
| [OSCNAME:device_info](#oscnamedevice_info) | Read-only device metadata |
| [OSCNAME:ON](#oscnameon) | Power control |
| [OSCNAME:ON:fbk](#oscnameonfbk) | Power state feedback |
| [OSCNAME:log](#oscnamelog) | Driver log level |
| [OSCNAME:resolution](#oscnameresolution) | ADC resolution setting |
| [OSCNAME:resolution:fbk](#oscnameresolutionfbk) | Current ADC resolution |

### [Data Capture](#data-capture-1)
| PV | Description |
|----|-------------|
| [OSCNAME:down_sample_ratio_mode](#oscnamedown_sample_ratio_mode) | Downsampling mode |
| [OSCNAME:down_sample_ratio_mode:fbk](#oscnamedown_sample_ratio_modefbk) | Downsampling mode feedback |
| [OSCNAME:down_sample_ratio](#oscnamedown_sample_ratio) | Manual downsampling ratio |
| [OSCNAME:down_sample_ratio:fbk](#oscnamedown_sample_ratiofbk) | Feedback of current downsampling ratio |
| [OSCNAME:num_samples](#oscnamenum_samples) | Total samples to capture |
| [OSCNAME:num_samples:fbk](#oscnamenum_samplesfbk) | Feedback: actual sample count |
| [OSCNAME:trigger_position_ratio](#oscnametrigger_position_ratio) | Trigger position (0–1) |
| [OSCNAME:trigger_position_ratio:fbk](#oscnametrigger_position_ratiofbk) | Feedback: actual trigger position |
| [OSCNAME:auto_trigger_us](#oscnameauto_trigger_us) | Auto-trigger timeout (µs) |
| [OSCNAME:waveform:start](#oscnamewaveformstart) | Start waveform acquisition |
| [OSCNAME:waveform:stop](#oscnamewaveformstop) | Stop acquisition |
| [OSCNAME:num_subwaveforms:fbk](#oscnamenum_subwaveformsfbk) | Number of waveforms per trigger |
| [OSCNAME:CH[A-D]:waveform](#oscnamecha-dwaveform) | Captured waveform |

### [Timing and Scaling](#timing-and-scaling-1)
| PV | Description |
|----|-------------|
| [OSCNAME:time_per_division:unit](#oscnametime_per_divisionunit) | Time/div unit (e.g., ms) |
| [OSCNAME:time_per_division:unit:fbk](#oscnametime_per_divisionunitfbk) | Feedback: unit |
| [OSCNAME:num_divisions](#oscnamenum_divisions) | Horizontal divisions |
| [OSCNAME:num_divisions:fbk](#oscnamenum_divisionsfbk) | Feedback: horizontal divisions |
| [OSCNAME:time_per_division](#oscnametime_per_division) | Time per division |
| [OSCNAME:time_per_division:fbk](#oscnametime_per_divisionfbk) | Feedback: actual value |
| [OSCNAME:sample_interval:fbk](#oscnamesample_intervalfbk) | Sample interval |
| [OSCNAME:sample_rate:fbk](#oscnamesample_ratefbk) | Sampling rate |
| [OSCNAME:timebase:fbk](#oscnametimebasefbk) | Internal timebase code |

### [Trigger Configuration](#trigger-configuration-1)
| PV | Description |
|----|-------------|
| [OSCNAME:trigger:channel](#oscnametriggerchannel) | Trigger source channel |
| [OSCNAME:trigger:channel:fbk](#oscnametriggerchannelfbk) | Feedback: trigger channel |
| [OSCNAME:trigger:type](#oscnametriggertype) | Trigger type (edge, window...) |
| [OSCNAME:trigger:direction](#oscnametriggerdirection) | Trigger edge direction |
| [OSCNAME:trigger:direction:fbk](#oscnametriggerdirectionfbk) | Feedback: trigger direction |
| [OSCNAME:trigger:mode:fbk](#oscnametriggermodefbk) | Feedback: trigger mode |
| [OSCNAME:trigger:upper:threshold](#oscnametriggerupperthreshold) | Upper level threshold |
| [OSCNAME:trigger:upper:threshold:fbk](#oscnametriggerupperthresholdfbk) | Feedback: upper threshold |
| [OSCNAME:trigger:upper:threshold:hysteresis](#oscnametriggerupperthresholdhysteresis) | Upper hysteresis |
| [OSCNAME:trigger:upper:threshold:hysteresis:fbk](#oscnametriggerupperthresholdhysteresisfbk) | Feedback: upper hysteresis |
| [OSCNAME:trigger:lower:threshold](#oscnametriggerlowerthreshold) | Lower level threshold |
| [OSCNAME:trigger:lower:threshold:fbk](#oscnametriggerlowerthresholdfbk) | Feedback: lower threshold |
| [OSCNAME:trigger:lower:threshold:hysteresis](#oscnametriggerlowerthresholdhysteresis) | Lower hysteresis |
| [OSCNAME:trigger:lower:threshold:hysteresis:fbk](#oscnametriggerlowerthresholdhysteresisfbk) | Feedback: lower hysteresis |
| [OSCNAME:trigger:interval](#oscnametriggerinterval) | Trigger polling interval |
| [OSCNAME:trigger:missed](#oscnametriggermissed) | Number of missed triggers |
| [OSCNAME:trigger_pulse_width](#oscnametrigger_pulse_width) | Pulse width in µs |
| [OSCNAME:trigger_pulse_width:fbk](#oscnametrigger_pulse_widthfbk) | Feedback: pulse width |

### [Channel Configuration](#channel-configuration-1)
| PV | Description |
|----|-------------|
| [OSCNAME:CH[A-D]:ON](#oscnamecha-don) | Enable input channel |
| [OSCNAME:CH[A-D]:ON:fbk](#oscnamecha-donfbk) | Feedback: channel enabled |
| [OSCNAME:CH[A-D]:coupling](#oscnamecha-dcoupling) | Input coupling (AC/DC) |
| [OSCNAME:CH[A-D]:coupling:fbk](#oscnamecha-dcouplingfbk) | Feedback: coupling |
| [OSCNAME:CH[A-D]:range](#oscnamecha-drange) | Voltage range |
| [OSCNAME:CH[A-D]:range:fbk](#oscnamecha-drangefbk) | Feedback: range |
| [OSCNAME:CH[A-D]:bandwidth](#oscnamecha-dbandwidth) | Bandwidth limiter |
| [OSCNAME:CH[A-D]:bandwidth:fbk](#oscnamecha-dbandwidthfbk) | Feedback: bandwidth |
| [OSCNAME:CH[A-D]:analog_offset](#oscnamecha-danalog_offset) | Analog offset voltage |
| [OSCNAME:CH[A-D]:analog_offset:fbk](#oscnamecha-danalog_offsetfbk) | Feedback: analog offset |
| [OSCNAME:CH[A-D]:analog_offset:max](#oscnamecha-danalog_offsetmax) | Max allowed offset |
| [OSCNAME:CH[A-D]:analog_offset:min](#oscnamecha-danalog_offsetmin) | Min allowed offset |
---
---
## PVs
### Device Control
### OSCNAME:device_info
- **Type**: `stringin`
- **Description**: Retrieves the model and serial number of the connected picoscope. 
- **Fields**:
  - **VAL**: The model and serial number of the device (string).
- **Example**:
  ```bash
  # To read the device information:
  $ caget OSC1234-01:device_info
  ```

### OSCNAME:ON
- **Type**: `bo`
- **Description**: Turns picoscope on/off.
- **Fields**:
  - `VAL`: Status of the picoscope.  
    | VAL   | Enum          | Description               |  
    |-------|---------------|---------------------------|  
    | 0     | OFF           | Disconnect the Picoscope. |  
    | 1     | ON            | Connect the Picoscope.    |  
- **Example**:
  ```bash
  # Connect the Picoscope by number
  $ caput OSC1234-01:ON 1
  # Disconnect the Picoscope by string 
  $ caput OSC1234-01:ON OFF

  # Get connection status
  $ caget OSC1234-01:ON
  ```

### OSCNAME:ON:fbk 
- **Type**: `bi` 
- **Description**: The actual state of the device.
  - Updates every 5 seconds with a ping to the device. 
- **Fields**: 
  - `VAL`: See `OSCNAME:ON`.

### OSCNAME:log 
- **Type**: `waveform`
- **Description**: A waveform PV that reports a message when an error occurs in the driver.
- **Example**:
  ```bash
  # To monitor log messages:
  $ camonitor OSC1234-01:log -S
  ```


### OSCNAME:resolution
- **Type**: `mbbo`  
- **Description**: Sets the resolution of the ADC (analog-to-digital converter), affecting signal precision. Applies globally to all active channels.

| VAL | Enum           | Description                     |
|-----|----------------|---------------------------------|
| 0   | PICO_DR_8BIT   | 8-bit resolution (256 levels)   |
| 1   | PICO_DR_10BIT  | 10-bit resolution (1024 levels) |
| 2   | PICO_DR_12BIT  | 12-bit resolution (4096 levels) |

> **Note**  
> Due to hardware constraints, **12-bit resolution** only supports single or the following dual-channel combinations:

| Valid 12-bit Channel Pairs |
|----------------------------|
| A + C                      |
| A + D                      |
| B + C                      |
| B + D                      |


**Example**:
```bash
# Set resolution to 10-bit
caput OSC1234-01:resolution PICO_DR_10BIT

# Check current resolution
caget OSC1234-01:resolution
```

### OSCNAME:resolution:fbk 
- **Type:** `mbbi`
- **Description:** The actual value of the resolution set to the device. 
  - Updated when a new value is set to `OSCNAME:resolution`. 
- **Fields:** 
  - `VAL`: See `OSCNAME:resolution`

---

### Data Capture
### OSCNAME:down_sample_ratio_mode
- **Type**: `mbbo`
- **Description**: The methods of data reduction, or downsampling.
- **Fields**:
  - `VAL`: Method of data reduction/downsampling.  
    | VAL  | Enum                   | Description                          |  
    |------|------------------------|--------------------------------------|  
    | 0    | AGGREGATE              |  Reduces every block of n values to just two values: a minimum and a maximum. The minimum and maximum values are returned in two separate buffers.                     |  
    | 1    | DECIMATE               |  Reduces every block of n values to a single value representing the average (arithmetic mean) of all the values.                   |  
    | 2    | AVERAGE                |  Reduces every block of n values to just the first value in the block, discarding all the other values                   |
    | 3    | TRIG_DATA_FOR_TIME_CALC (*NOT IMPLEMENTED*)|  In overlapped mode only, causes trigger data to be retrieved from the scope to calculate the trigger time without requiring a user buffer to be set for this data.
    | 4    | TRIGGER (*NOT IMPLEMENTED*) |  Gets 20 samples either side of the trigger point. When using trigger delay, this is the original event causing the trigger and not the delayed point. This data is available even when the original trigger point falls outside the main preTrigger + postTrigger data. Trigger data must be retrieved before attempting to get the trigger time.
    | 5    | RAW                    |  No downsampling. Returns raw data values|
- **Example**:
  ```bash
    # Set down sample mode to AGGREGATE by number
    $ caput OSC1234-01:down_sample_ratio_mode 0
    # Set down sample mode to RAW by enum 
    $ caput OSC1234-01:down_sample_ratio_mode RAW

    # Get current down sampling mode
    $ caget OSC1234-01:down_sample_ratio_mode
  ```

### OSCNAME:down_sample_ratio_mode:fbk 
- **Type**: `mbbi`
- **Description**: The method of data reduction applied to the last waveform acquired. 
  - Updated at the time `OSCNAME:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:down_sample_ratio_mode`. 

### OSCNAME:down_sample_ratio
- **Type**: `ao`
- **Description**: The downsampling factor that will be applied to the raw data. 
- **Fields**:
  - `VAL`: The downsampling factor that will be applied to the raw data. Must be greater than zero.  
  
- **Example**:
  ```bash
    # Set down sample ratio to 1
    $ caput OSC1234-01:down_sample_ratio 1
  
    # Get current down sampling ratio
    $ caget OSC1234-01:down_sample_ratio
  ```

### OSCNAME:down_sample_ratio:fbk 
- **Type**: `ai`
- **Description**: The downsampling factor that has been applied to the raw data of the last waveform acquired. 
  - Updated at the time `OSCNAME:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:down_sample_ratio`. 

### OSCNAME:num_samples
- **Type**: `ao`
- **Description**: The number of samples will be collected. Applies to all channels.
- **Fields**:
  - `VAL`: The number of samples in integer.
  
    Maximum sample size
    |                    | 8 BIT      | 10 BIT     |  
    |--------------------|------------|------------|  
    | ONE Channel ON     | 4294966784 | 2147483392 |  
    | TWO Channels ON    | 2147483392 | 1073741696 |  
    | THREE Channels ON  | 1073741696 | N/A        |  
    | FOUR Channels ON   | 1073741696 | N/A        |
- **Example**:
  ```bash
    # Collect 100000000 samples 
    $ caput OSC1234-01:num_samples 100000000
    # Collect 1000 samples 
    $ caput OSC1234-01:num_samples 1000

    # Get sample size
    $ caget OSC1234-01:num_samples
  ```

### OSCNAME:num_samples:fbk 
- **Type**: `ai`
- **Description**: The number of samples collected for the last waveform acquired. 
  - Updated at the time `OSCNAME:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:num_samples`. 

### OSCNAME:trigger_position_ratio
- **Type**: `ao`
- **Description**: A value between 0 and 100 that determines the position of the trigger point in the acquisition window.
- **Fields**:
  - `VAL`: The ratio of pre-trigger to post-trigger samples.
    - 0: All samples are post-trigger (no pre-trigger).
    - 100: All samples are pre-trigger (no post-trigger).
    - 50: Equal pre-trigger and post-trigger samples (50% each).
  - `EGU`: The unit is `%`.

- **Example**:
  ```bash
    # Set trigger position ratio 80% pre-trigger samples, 20% post-trigger samples 
    $ caput OSC1234-01:trigger_position_ratio 80

    # Get trigger position ratio 
    $ caget OSC1234-01:trigger_position_ratio
  ```

### OSCNAME:trigger_position_ratio:fbk 
- **Type**: `ai`
- **Description**: The position of the trigger in the last acquired waveform.  
  - Updated at the time `OSCNAME:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger_position_ratio`.

### OSCNAME:auto_trigger_us 
- **Type**: `ao` 
- **Description**: The time in microseconds for which the scope will wait before collecting data if no trigger event occurs. If set to zero, the scope will wait indefinitely for a trigger. 

### OSCNAME:waveform:start
- **Type**: `bo`
- **Description**: Starts waveform acquisition using the current configuration (as set by other PVs).
- **Fields**:
  - `VAL`: Start getting the waveform.
    | VAL   | Description      |
    |-------|------------------|
    | 1     | Start acquiring waveform   |
- **Example**:
  ```bash
    # Start acquiring waveform
    $ caput OSC1234-01:CHA:waveform:start 1
  ```

### OSCNAME:waveform:stop
- **Type**: `bo`
- **Description**: Stops waveform acquisition in progress.
- **Fields**:
  - `VAL`: Start getting the waveform.
    | VAL   | Description      |
    |-------|------------------|
    | 1     | Stop acquiring waveform   |
- **Example**:
  ```bash
    # Stop acquiring waveform
    $ caput OSC1234-01:CHA:waveform:stop 1
  ```

### OSCNAME:num_subwaveforms:fbk
- **Type**: `ai`
- **Description**: This PV represents the number of subwaveforms that compose a single waveform.
- **Fields**:
  - `VAL`: Number of subwaveforms.
- **Example**:
  ```bash
    $ caget OSC1234-01:num_subwaveforms:fbk
  ```

### OSCNAME:CH[A-D]:waveform
- **Type**: `waveform`
- **Description**: Waveform will be available after `OSCNAME:waveform:start` PV is invoked.
- **Fields**:
  - `VAL`: Waveform result acquired
- **Example**:
  ```bash
    # Get waveform result
    $ caget OSC1234-01:CHA:waveform
  ```
- **Note**: The raw value from the waveform is a scaled value. To interpret the waveform:
    | **Resolution**          | 8 BIT         | 10 BIT        | 12 BIT        |
    |-------------------------|---------------|---------------|---------------|
    | **Voltage Range Scale** | $\pm 32,512$  | $\pm 32,704$  | $\pm 32,736$  |

  - **Calculation**:
    - The actual voltage is calculated as:
      $$\text{Actual Voltage} = \text{Voltage Range (in Volts)} \times \frac{\text{Raw Waveform Value}}{\text{Voltage Range (in Scale Units)}}$$

  - **Example**:
    - **Resolution**: $`8 \text{Bits}`$
    - **Voltage Range (in Volts)**: $`\pm 20  \text{V}`$
    - **Raw Waveform Value**: $`8129`$
    - **Voltage Range (in Scale Units)**: $`\pm 32,512`$.
      $$\text{Actual Voltage} = 20 \text{V} \times \frac{8192}{32512} = 5.04 \text{V}$$

---
### Timing and Scaling
### OSCNAME:time_per_division:unit 
- **Type**: `mbbo` 
- **Description**: The time unit used per division. 
- **Fields**:
  - `VAL`
    | VAL   | Description |
    |-------|-------------|
    | 0     | ns/div      |
    | 1     | us/div      |
    | 2     | ms/div      | 
    | 3     | s/div       |

### OSCNAME:time_per_division:unit:fbk
- **Type**: `mbbi` 
- **Description**: The currently set time unit used per division. 
- **Fields**:
  - `VAL`: See `OSCNAME:time_per_division:unit`

### OSCNAME:num_divisions 
- **Type**: `ao` 
- **Description**: The number of divisions. Defaults to 10 divisions.

### OSCNAME:num_divisions:fbk
- **Type**: `ai` 
- **Description**: The currently set number of divisions.

### OSCNAME:time_per_division
- **Type**: `mbbo` 
- **Description**: The time per division. 
- **Fields**:
- If `OSC:time_per_division:unit` = s/div:
    | VAL   | Enum  | Description    |
    |-------|-------|----------------|
    | 0     |   1   | 1    time/div  |
    | 1     |   2   | 2    time/div  | 
    | 2     |   5   | 5    time/div  |  
    | 3     |   10  | 10   time/div  | 
- Otherwise, 
    | VAL   | Enum  | Description    |
    |-------|-------|----------------|
    | 0     |  1    | 1    time/div  |
    | 1     |  2    | 2    time/div  | 
    | 2     |  5    | 5    time/div  |  
    | 3     |  10   | 10   time/div  | 
    | 4     |  20   | 20   time/div  | 
    | 5     |  50   | 50   time/div  | 
    | 6     |  100  | 100  time/div  | 
    | 7     |  200  | 200  time/div  | 
    | 8     |  500  | 500  time/div  | 
    | 9     |  1000 | 1000 time/div  | 
    | 10    |  2000 | 2000 time/div  | 
    | 11    |  5000 | 5000 time/div  |

### OSCNAME:time_per_division:fbk
- **Type**: `mbbi` 
- **Description**: The currently set time per division. 
- **Fields**:
  - `VAL`: See `OSCNAME:time_per_division`

### OSCNAME:sample_interval:fbk
- **Type**: `ai`
- **Description**: The sample interval in seconds that will be applied when capturing data.
  - The value returned is calculated from `OSCNAME:num_samples`,  `OSCNAME:time_per_division:fbk`,      `OSCNAME:time_per_division:unit:fbk`, and `OSCNAME:num_divisions`. 
- **Fields**: 
  - `VAL`: The actual sample interval applied in seconds.

### OSCNAME:sample_rate:fbk 
- **Type**: `ai`
- **Description**: The sample rate in samples/second (S/s).
  - The value returned is based on the value of `OSCNAME:time_per_division:fbk`,      `OSCNAME:time_per_division:unit:fbk`, and `OSCNAME:num_divisions`.

### OSCNAME:timebase:fbk
- **Type**: `ai`
- **Description**: The time scale used to determine time per division when capturing data. 
  - The value returned is based on the value of `OSCNAME:time_per_division:fbk` and     `OSCNAME:time_per_division:unit:fbk`. 
- **Fields**:
  - `VAL`: Timebase - *from PicoScope 6000 Series (A API) Programmer's Guide*
  ![image](https://github.lightsource.ca/cid/DRIVER_Picoscope6000ESeries/assets/209/a348ee4f-d014-44ec-a948-070051ce5d46)

    _Minimum Timebase:_
    |                   | 8 BIT      | 10 BIT     |  
    |-------------------|------------|------------|  
    | One Channel       | 0 (200 ps) | 2 (800 ps) |  
    | Two Channels      | 0 (200 ps) | 2 (800 ps) |  
    | Three Channels    | 1 (400 ps) | N/A        |  
    | Four Channels     | 1 (400 ps) | N/A        |


---
### Trigger Configuration
### OSCNAME:trigger:channel
- **Type**: `mbbo`
- **Description**: The source channel of triggering.
- **Fields**:
  - `VAL`: The channel will be used by triggering.
    |VAL      |Enum         |Description |  
    |---------|-------------|------------|  
    | 0       | CHANNEL_A   | Use Channel A as the triggering source |  
    | 1       | CHANNEL_B   | Use Channel B as the triggering source |  
    | 2       | CHANNEL_C   | Use Channel C as the triggering source        |  
    | 3       | CHANNEL_D   | Use Channel D as the triggering source        |
    | 4       | TRIGGER_AUX | Use Auxiliary trigger input as the triggering source, with a fixed threshold of 1.25 V (nominal) to suit 2.5 V CMOS|
    | 10       | NONE        | No trigger set |

>**Note:**
>AUX trigger input requires pulse width >50 ns for detection. 
>Shorter pulses may not be reliably captured.

- **Example**:
  ```bash
    # Set triggering source to Auxiliary trigger input
    $ caput OSC1234-01:trigger:channel TRIGGER_AUX

    # Get triggering source channel
    $ caget OSC1234-01:trigger:channel
  ```

### OSCNAME:trigger:channel:fbk 
- **Type**: `mbbi`
- **Description**: The feedback PV of `OSCNAME:trigger:channel`
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger:channel`. 

### OSCNAME:trigger:type 
- **Type**: `mbbo`
- **Description**: The type of trigger set. 
- **Fields**: 
    - `VAL`:

| VAL | Enum           | Description                                                                                                        | Threshold Used   |
|-----|----------------|--------------------------------------------------------------------------------------------------------------------|-------------|
| 0   | NO TRIGGER     | No trigger set, streaming mode.                                                                                    | None        |
| 1   | SIMPLE EDGE    | Simple trigger, monitors incoming signal and waits for the voltage to rise above (or fall below) a set threshold.  | Upper       |
| 2   | WINDOW         | Detects the moment when the waveform enters or leaves a voltage range.                                             | Upper/Lower |
| 3   | ADVANCED EDGE  | Provides rising, falling, and dual edge conditions, as well as adjustable hysteresis.                              | Upper       |

### OSCNAME:trigger:direction
- **Type**: `mbbo`
- **Description**: The direction of the trigger event.The directions available change based on the value set to `OSC:trigger:type`. 
- **Fields**:
  - `VAL`: The direction of the trigger event.

  - If `OSC:trigger:type` = NO TRIGGER: 
      | VAL | Enum                  | Description                                         | Threshold Used |
      |-----|-----------------------|-----------------------------------------------------|-----------|
      | 0   | NONE                  | No trigger set.                                     | None      | 
  - If `OSC:trigger:type` = SIMPLE EDGE: 
      | VAL | Enum                  | Description                                         | Threshold Used |
      |-----|-----------------------|-----------------------------------------------------|-----------|
      | 0   | RISING                | Triggers when rising edge crosses upper threshold.  | Upper     | 
      | 1   | FALLING               | Triggers when falling edge crosses upper threshold. | Upper     |
  - If `OSC:trigger:type` = WINDOW:  
      | VAL | Enum            | Description                                                                                 | Threshold Used  |
      |-----|-----------------|---------------------------------------------------------------------------------------------|-------------|
      | 0   | ENTER           | Triggers when the signal enters the range between the upper and lower thresholds.           | Upper/Lower | 
      | 1   | EXIT            | Triggers when the signal exits the range between the upper and lower thresholds.            | Upper/Lower | 
      | 2   | ENTER OR EXIT   | Triggers when the signal enters or exits the range between the upper and lower thresholds.  | Upper/Lower |
  - If `OSC:trigger:type` = ADVANCED EDGE:   
      | VAL | Enum              | Description                                                  | Threshold Used | 
      |-----|-------------------|--------------------------------------------------------------|-----------|
      | 0   | RISING            | Triggers when rising edge crosses upper threshold.           | Upper     | 
      | 1   | FALLING           | Triggers when falling edge crosses upper threshold.          | Upper     |
      | 2   | RISING OR FALLING | Triggers when falling or rising edge crosses upper threshold | Upper     |

### OSCNAME:trigger:direction:fbk 
- **Type**: `mbbi`
- **Description**: The feedback PV of `OSCNAME:trigger:direction`
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger:direction`. 

### OSCNAME:trigger:mode:fbk
- **Type**: `mbbi`
- **Description**: The mode of triggering, determined by `OSC:trigger:type`. 
- **Fields**:
  - `VAL`: The mode of triggering.
    |VAL      |Enum         |Description                   | Threshold  |
    |---------|-------------|------------------------------|------------|  
    | 0       | LEVEL       | Will only use one threshold  | Upper      | 
    | 1       | WINDOW      | Will use two thresholds      | Lower      |

### OSCNAME:trigger:upper:threshold
- **Type**: `ao`
- **Description**: The trigger threshold in volts. Used as only threshold in SIMPLE EDGE and ADVANCED EDGE trigger types and used as upper limit when using WINDOW trigger type. 
- **Fields**:
  - `VAL`: The upper threshold in Volts.

- **Example**:
  ```bash
    # Set triggering upper threshold to 5 Volts
    $ caput OSC1234-01:trigger:upper:threshold 5

    # Get triggering upper threshold
    $ caget OSC1234-01:trigger:upper:threshold
  ```

### OSCNAME:trigger:upper:threshold:fbk 
- **Type**: `ai`
- **Description**: The feedback PV of `OSCNAME:trigger:upper:threshold`
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger:upper:threshold`.

### OSCNAME:trigger:upper:threshold:hysteresis
- **Type**: `ao`
- **Description**: The % hysteresis applied to the upper threshold value.

### OSCNAME:trigger:upper:threshold:hysteresis:fbk
- **Type**: `ao`
- **Description**: The % hysteresis applied to the upper threshold value.

### OSCNAME:trigger:lower:threshold
- **Type**: `ao`
- **Description**: The lower trigger threshold in volts. Only used when trigger type is WINDOW. 
- **Fields**:
  - `VAL`: Lower threshold in volts. 

### OSCNAME:trigger:lower:threshold:fbk 
- **Type**: `ai`
- **Description**: The feedback PV of `OSCNAME:trigger:lower`
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger:lower`.

### OSCNAME:trigger:lower:threshold:hysteresis
- **Type**: `ao`
- **Description**: The % hysteresis applied to the lower threshold value.

### OSCNAME:trigger:lower:threshold:hysteresis:fbk
- **Type**: `ao`
- **Description**: The % hysteresis applied to the lower threshold value.

### OSCNAME:trigger:interval 
- **Type**: `ai`
- **Description**: The trigger frequency in seconds. 

### OSCNAME:trigger:missed 
- **Type**: `ai`
- **Description**: The number of triggers missed since the last detected trigger and successful data capture. Such as triggers that occurred during data capture. 

### OSCNAME:trigger_pulse_width
- **Type**: `ao` 
- **Description**: The trigger signal pulse width. 
- **Fields**:
  - `VAL`: The duration of a trigger signal in seconds, when this value is set, allows the software to dynamically adjust the number of samples collected to ensure the time interval does not significantly exceed the trigger signal duration. When set to 0, the dynamic adjustment of the sample count is disabled. Default to 50 ns.

### OSCNAME:trigger_pulse_width:fbk:
- **Type**: `ai` 
- **Description**: The trigger signal pulse width set. 
- **Fields**:
  - `VAL`: The duration of a trigger signal in seconds, when this value is set, allows the software to dynamically adjust the number of samples collected to ensure the time interval does not significantly exceed the trigger signal duration. When set to 0, the dynamic adjustment of the sample count is disabled. Default to 50 ns.

---
### Channel Configuration
### OSCNAME:CH[A-D]:ON
- **Type**: `bo`
- **Description**: Switches a specific Picoscope channel on and off
- **Fields**:
  - **VAL**: The state of the channel (ON/OFF). 
    | VAL   | Description                   |
    |-------|-------------------------------|
    | 0     | The channel is switched off  |
    | 1     | The channel is switched on   |
- **Example**:
  ```bash
    # Switch on Channel A by number 
    $ caput OSC1234-01:CHA:ON 1
    # Switch off Channel A by string
    $ caput OSC1234-01:CHA:ON OFF

    # Get state of Channel A:
    $ caget OSC1234-01:CHA:ON
  ```

### OSCNAME:CH[A-D]:ON:fbk 
- **Type**: `bo`
- **Description**: The actual state of the channel.
  - Updated when a new value set to `OSCNAME:CH[A-D]:ON` and when a channel configuration is changed. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-D]:ON` 

### OSCNAME:CH[A-D]:coupling
- **Type**: `mbbo`
- **Description**: The impedance and coupling type. 
- **Fields**:
  - `VAL`: The impedance and coupling type.
    | VAL   | Enum      | Description                   |
    |-------|-----------|-------------------------------|
    | 0     | PICO_AC   | 1 mΩ impedance, AC coupling.  |
    | 1     | PICO_DC   | 1 mΩ impedance, DC coupling.  |
- **Example**:
  ```bash
    # Set coupling type to PICO_AC by number 
    $ caput OSC1234-01:CHA:coupling 0
    # Set coupling type to PICO_DC by enum 
    $ caput OSC1234-01:CHA:coupling PICO_DC

    # Get coupling type
    $ caget OSC1234-01:CHA:coupling
  ```

### OSCNAME:CH[A-D]:coupling:fbk
- **Type**: `mbbi`
- **Description**: The actual impedance and coupling type set to a channel. 
  - **Note**: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-D]:coupling` 

### OSCNAME:CH[A-D]:range
- **Type**: `mbbo`
- **Description**:  
- **Fields**:
  - `VAL`: The value of the voltage range.
    | VAL   | Enum    | Description                         |
    |-------|---------|-----------------------------------|
    | 0     |  10MV   | Voltage range from -10 mV to 10 mV  |
    | 1     |  20MV   | Voltage range from -20 mV to 20 mV  |
    | 2     |  50MV   | Voltage range from -50 mV to 50 mV  |
    | 3     |  100MV  | Voltage range from -100 mV to 100 mV |
    | 4     |  200MV  | Voltage range from -200 mV to 200 mV |
    | 5     |  500MV  | Voltage range from -500 mV to 500 mV |
    | 6     |  1V     | Voltage range from -1 V to 1 V       |
    | 7     |  2V     | Voltage range from -2 V to 2 V       |
    | 8     |  5V     | Voltage range from -5 V to 5 V       |
    | 9     |  10V    | Voltage range from -10 V to 10 V     |
    | 10    |  20V    | Voltage range from -20 V to 20 V     |
    | 11    |  50V    | Voltage range from -50 V to 50 V     |
    | 12    |  100V   | Voltage range from -100 V to 100 V   |
    | 13    |  200V   | Voltage range from -200 V to 200 V   |
    | 14    |  500V   | Voltage range from -500 V to 500 V   |
    | 15    |  1KV    | Voltage range from -1000 V to 1000 V |
- **Example**:
  ```bash
    # Set voltage range to -/+20V by number 
    $ caput OSC1234-01:CHA:range 10
    # Set voltage range to 500 mV MHZ by enum
    $ caput OSC1234-01:CHA:range PICO_X1_PROBE_500MV

    # Get voltage range
    $ caget OSC1234-01:CHA:range
  ```

### OSCNAME:CH[A-D]:range:fbk
- **Type**: `mbbi`
- **Description**: The actual value of the voltage range set to a channel.  
  - **Note**: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-D]:range` 

### OSCNAME:CH[A-D]:bandwidth
- **Type**: `mbbo`
- **Description**: Controls the analog bandwidth of the channel. Set before initiating waveform acquisition.
- **Fields**:
  - `VAL`: Trigger to start getting the waveform.
    | VAL   | Enum        | Description              |
    |-------|-------------|--------------------------|
    | 0     | BW_FULL     | Full bandwidth (default)  |
    | 1     | BW_20MHZ    | Bandwidth of 20 MHZ       |
- **Example**:
  ```bash
    # Set bandwidth to full bandwidth by number
    $ caput OSC1234-01:CHA:bandwidth 0
    # Set bandwidth to 20 MHz using enum
    $ caput OSC1234-01:CHA:bandwidth PICO_BW_20MHZ

    # Get bandwidth
    $ caget OSC1234-01:CHA:bandwidth
  ```

### OSCNAME:CH[A-D]:bandwidth:fbk
- **Type**: `mbbi`
- **Description**: The actual value of the voltage range set to a channel.  
  - NOTE: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-D]:bandwidth` 

### OSCNAME:CH[A-D]:analog_offset
- **Type**: `ao`
- **Description**: A voltage to add to the input channel before digitization.
- **Fields**:
  - `VAL`: The value of the voltage offset.
- **Example**:
  ```bash
    # Set offset to 1V
    $ caput OSC1234-01:CHA:analog_offset 1
    # Set offset to 10V
    $ caput OSC1234-01:CHA:analog_offset 10

    # Get offset
    $ caput OSC1234-01:CHA:analog_offset
  ```

### OSCNAME:CH[A-D]:analog_offset:fbk
- **Type**: `ai`
- **Description**: The actual voltage added to the input channel before digitization. The analog offset voltage had limits which depend on the voltage range and coupling set to a channel. If the value put to `OSCNAME:CH[A-D]:analog_offset` is outside of the limits, the max or min value will be used and will be reported by this PV. 
  - **Note**: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-D]:analog_offset` 

### OSCNAME:CH[A-D]:analog_offset:max 
- **Type**: `ai`
- **Description**: The maximum allowed analog offset voltage allowed for the range. 
  - Updated when the value of `OSCNAME:CH[A-D]:range` or `OSCNAME:CH[A-D]:coupling` are changed. 

### OSCNAME:CH[A-D]:analog_offset:min
- **Type**: `ai`
- **Description**: The minimum allowed analog offset voltage allowed for the range. 
  - Updated when the value of `OSCNAME:CH[A-D]:range` or `OSCNAME:CH[A-D]:coupling` are changed. 

---
---
# For Developer
## PicoScope 6000 Series (A API) Programmer's Guide - Pico Technology Ltd.
https://www.picotech.com/download/manuals/picoscope-6000-series-a-api-programmers-guide.pdf
## Threading Hierarchy

- **acquisition_thread_function**: *Drives acquisition, updates EPICS PVs.*
  - **run_block_capture**: *Captures single-shot block data (times/div < 100 ms/div).*
  - **run_stream_capture**: *Manages streaming, spawns per-channel threads.*
    - **channel_streaming_thread_function**: *Processes one channel’s streaming data, updates PVs.*

## Control Logic

- **Main Acquisition Thread**:
  - Internally, OSCNAME:waveform:start signals the acquisition thread to begin capture. This thread determines whether to use block or streaming mode based on waveform size
  - **Lifecycle**: Runs indefinitely (immortal).
  - **Start**: Waits for `acquisitionStartEvent` to begin acquisition.
  - **Stop**: Sets `dataAcquisitionFlag` to `FALSE` to halt looping and wait on `acquisitionStartEvent`.
  - **Mode**: Selects block (`subwaveform_num == 0`) or streaming (`subwaveform_num > 0`) mode.

- **Channel Streaming Threads**:
  - Create a new thread for **each** channel opened.
  - **Lifecycle**: Runs until all subwaveforms for one waveform are collected on all open channels.
  - **Behavior**: Processes channel data, updates waveform PVs per subwaveform.
  - **Stop**: Exits if `dataAcquisitionFlag` is `FALSE`, signals completion.

- **Block Capture**:
  - **Behavior**: Captures one-time block data for all enabled channels, returns data for PV updates.
  - **Stop**: Halts on errors or if `dataAcquisitionFlag` is `FALSE`.

- **Stream Capture**:
  - **Behavior**: Starts streaming, spawns channel threads, waits for thread completion.
  - **Stop**: Halts on errors or if `dataAcquisitionFlag` is `FALSE`.

## Troubleshooting

- **Problem**: No waveform data appears after `waveform:start`.
  - **Solution**: Check that `CH[A-D]:ON` is ON and trigger settings are valid.

- **Problem**: Cannot connect to PicoScope.
  - Ensure `udev_install.sh` was run and the device is connected before IOC startup.
  - Ensure `st.cmd` has the correct serial number.
## License

This project is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.en.html).  
You may use, modify, and distribute it under the terms of the GPLv3.

See the full license text in the [LICENSE](https://github.com/Canadian-Light-Source/DRIVER_Picoscope6000ESeries/blob/main/LICENSE) file.

> ⚠️ **Third-Party Dependency Notice**  
> This project depends on SDK files (`libps6000a`, header files such as `ps6000aApi.h`) provided by Pico Technology Ltd.  
> These files are **not licensed under the GPL** and are not redistributed with this project.  
> You must install them separately via the [PicoScope 7 for Linux package](https://www.picotech.com/downloads/linux).
