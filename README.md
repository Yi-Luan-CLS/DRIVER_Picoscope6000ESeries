# DRIVER_Picoscope6000ESeries
## Overview
This document provides detailed information about the EPICS driver for the Picoscope 6000E Series oscilloscope. The driver allows control and monitoring of the oscilloscope through EPICS Process Variables (PVs).

---
## Usage
- The picoscope to open is identified by the SERIAL_NUM macro in the st.cmd file. This macro must equal the serial number of the PicoScope you wish to connect to. 
- It is expected that a PicoScope is connected at application start-up. If a PicoScope is connected after the application has started, setting `<OSCNAME>:ON` to `1` or `ON` will open the scope.  

- The following Process Variables (PVs) are used to configure a channel:  
  - `<OSCNAME>:CH[A-D]:coupling`  
  - `<OSCNAME>:CH[A-D]:range`  
  - `<OSCNAME>:CH[A-D]:bandwidth`  
  - `<OSCNAME>:CH[A-D]:analog_offset`    
>[!Note] 
>Changes to the above PVs will turn the channel ON. Changes apply immediately and can be verified by checking the :fbk PVs.  
>To ensure a channel is ON, verify the status with the feedback PV: `<OSCNAME>:CH[A-D]:ON:fbk`.

- **Simple usage example walkthrough:**

  The driver for PicoScope 6000E Series devices (libps6000a), from Pico Technology (https://www.picotech.com/downloads), is included in this repository. 

  This is a case using Channel B with a signal voltage within ±20V.

  ```bash
  git clone git@github.lightsource.ca:cid/DRIVER_Picoscope6000ESeries.git
  cd DRIVER_Picoscope6000ESeries/
  make clean all
  cd iocBoot/iocOPI2027-002/
  ./st.cmd

  # On another terminal
  caput OSC1022-01:CHB:range 10       # Set the voltage range for Channel B to +/-20V
  caput OSC1022-01:num_samples 1000   # Set the number of samples to 1,000
  caget OSC1022-01:CHB:ON:fbk         # Check if Channel B is ON. If not, try: caput OSC1022-01:CHB:ON ON. 
  caput OSC1022-11:sample_interval 0.001    # Set the sample time interval to 1 ms
  caput OSC1022-01:CHB:waveform:start 1 # Start the waveform capturing (Use external triggering)
  caget OSC1022-01:CHB:waveform         # when waveform is ready, get the waveform. The waveform has a maximum size of 1,000,000 elements. Only the first 10,000 elements will contain data; the rest will be zeros.
  ```
  All details of these configurations can be found in this document.

  **The waveform data is a scaled value. The calculation is located at the bottom.**
  - This will retrieve the waveform using the latest values of the data capture configuration PVs.    
  - To acquire a waveform for a specific channel, the PV `<OSCNAME>:CH[A-D]:ON` must be set to ON. Requesting `OSC1021-01:CHA:waveform:start` will fail if `OSC1021-01:CHA:ON` is set to OFF. 
  - The waveform data will be returned in the PV `<OSCNAME>:CH[A-D]:waveform`. 

>[!Note] 
>Data capture configuration PVs have :fbk PVs. These are updated with a put to `OSCNAME:CH[A-D]:waveform:start`. The value of the :fbk PVs contain the settings used to capture the LAST waveform.

---

## PVs
**_Oscilloscope configurations:_**
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


### OSCNAME:resolution
- **Type**: `mbbo`
- **Description**: The resolution of the sampling hardware in the Picoscope, providing varying levels of signal precision. Applied to all channels.
- **Fields**:
  - `VAL`: The resolution mode and corresponding levels.  
    | VAL   | Enum          | Description                          |  
    |-------|---------------|--------------------------------------|  
    | 0     | PICO_DR_8BIT  | 8-bit resolution (256 levels).        |  
    | 1     | PICO_DR_10BIT | 10-bit resolution (1024 levels).      |  
    | 2     | PICO_DR_12BIT | 12-bit resolution (4096 levels).      |
- **Example**:
  ```bash
    # Set resolution to PICO_DR_10BIT by number 
    $ caput OSC1234-01:resolution 1
    # Set resolution to PICO_DR_8BIT by enum 
    $ caput OSC1234-01:resolution PICO_DR_8BIT

    # Get resolution
    $ caget OSC1234-01:resolution
  ```
### OSCNAME:resolution:fbk 
- **Type:** `mbbi`
- **Description:** The actual value of the resolution set to the device. 
  - Updated when a new value is set to `OSCNAME:resolution`. 
- **Fields:** 
  - `VAL`: See `OSCNAME:resolution`

---

**_Data capture configurations:_**
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
    | 3    | TRIG_DATA_FOR_TIME_CALC|  In overlapped mode only, causes trigger data to be retrieved from the scope to calculate the trigger time without requiring a user buffer to be set for this data.
    | 4    | TRIGGER                |  Gets 20 samples either side of the trigger point. When using trigger delay, this is the original event causing the trigger and not the delayed point. This data is available even when the original trigger point falls outside the main preTrigger + postTrigger data. Trigger data must be retrieved before attempting to get the trigger time.
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
  - Updated at the time `OSCNAME:CH[A-D]:waveform:start` is set to 1. 
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
- **Description**: The downsampling facter that has been applied to the raw data of the last waveform acquired. 
  - Updated at the time `OSCNAME:CH[A-D]:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:down_sample_ratio`. 

### OSCNAME:num_samples
- **Type**: `ao`
- **Description**: The number of samples will be collected. Applies to all channels.
- **Fields**:
  - `VAL`: The number of samples in integer.
  
    Maximum sample size
    |                   | 8 BIT      | 10 BIT     |  
    |-------------------|------------|------------|  
    | ONE Channel       | 4294966784 | 2147483392 |  
    | TWO Channels      | 2147483392 | 1073741696 |  
    | THREE Channels    | 1073741696 | N/A        |  
    | FOUR Channels     | 1073741696 | N/A        |
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
  - Updated at the time `OSCNAME:CH[A-D]:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:num_samples`. 

### OSCNAME:trigger_position_ratio
- **Type**: `ao`
- **Description**: A value between 0 and 1 that determines the position of the trigger point in the acquisition window.
- **Fields**:
  - `VAL`: The ratio of pre-trigger to post-trigger samples.
    - 0: All samples are post-trigger (no pre-trigger).
    - 1: All samples are pre-trigger (no post-trigger).
    - 0.5: Equal pre-trigger and post-trigger samples (50% each).
- **Example**:
  ```bash
    # Set trigger position ratio 80% pre-trigger samples, 20% post-trigger samples 
    $ caput OSC1234-01:trigger_position_ratio 0.8

    # Get trigger position ratio 
    $ caget OSC1234-01:trigger_position_ratio
  ```

### OSCNAME:trigger_position_ratio:fbk 
- **Type**: `ai`
- **Description**: The position of the trigger in the last acquired waveform.  
  - Updated at the time `OSCNAME:CH[A-D]:waveform:start` is set to 1. 
- **Fields**: 
  - `VAL`: See `OSCNAME:trigger_position_ratio`. 

### OSCNAME:sample_interval 
- **Type**: `ao`
- **Description**: The requested sample interval in seconds. 
- **Fields**: 
  - `VAL`: The sample interval in seconds you would like to obtain. 

### OSCNAME:sample_interval:fbk
- **Type**: `ai`
- **Description**: The actual sample interval in seconds that will be applied when capturing data.
  - Updated with a new value put to `OSCNAME:sample_interval` or when any channel is turned ON/OFF. 
- **Fields**: 
  - `VAL`: The actual sample interval appliead in seconds. 

### OSCNAME:timebase:fbk
- **Type**: `ai`
- **Description**: The time scale used to determine time per division when capturing data. 
  - The value returned is based on the value of `OSCNAME:sample_interval:fbk`. 
  - This value will update when a new value is put to `OSCNAME:sample_interval` or when any channel is turned ON/OFF.
- **Fields**:
  - `VAL`: Timebase
  ![image](https://github.lightsource.ca/cid/DRIVER_Picoscope6000ESeries/assets/209/a348ee4f-d014-44ec-a948-070051ce5d46)

    _Minimum Timebase:_
    |                   | 8 BIT      | 10 BIT     |  
    |-------------------|------------|------------|  
    | One Channel       | 0 (200 ps) | 2 (800 ps) |  
    | Two Channels      | 0 (200 ps) | 2 (800 ps) |  
    | Three Channels    | 1 (400 ps) | N/A        |  
    | Four Channels     | 1 (400 ps) | N/A        | 

---
**_Channel configurations:_**
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
  - NOTE: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-B]:coupling` 

### OSCNAME:CH[A-D]:range
- **Type**: `mbbo`
- **Description**:  
- **Fields**:
  - `VAL`: The value of the voltage range.
    | VAL   | Enum                 | Description                         |
    |-------|----------------------|-----------------------------------|
    | 0     | PICO_X1_PROBE_10MV   | Voltage range from -10 mV to 10 mV  |
    | 1     | PICO_X1_PROBE_20MV   | Voltage range from -20 mV to 20 mV  |
    | 2     | PICO_X1_PROBE_50MV   | Voltage range from -50 mV to 50 mV  |
    | 3     | PICO_X1_PROBE_100MV  | Voltage range from -100 mV to 100 mV |
    | 4     | PICO_X1_PROBE_200MV  | Voltage range from -200 mV to 200 mV |
    | 5     | PICO_X1_PROBE_500MV  | Voltage range from -500 mV to 500 mV |
    | 6     | PICO_X1_PROBE_1V     | Voltage range from -1 V to 1 V       |
    | 7     | PICO_X1_PROBE_2V     | Voltage range from -2 V to 2 V       |
    | 8     | PICO_X1_PROBE_5V     | Voltage range from -5 V to 5 V       |
    | 9     | PICO_X1_PROBE_10V    | Voltage range from -10 V to 10 V     |
    | 10    | PICO_X1_PROBE_20V    | Voltage range from -20 V to 20 V     |
    | 11    | PICO_X1_PROBE_50V    | Voltage range from -50 V to 50 V     |
    | 12    | PICO_X1_PROBE_100V   | Voltage range from -100 V to 100 V   |
    | 13    | PICO_X1_PROBE_200V   | Voltage range from -200 V to 200 V   |
    | 14    | PICO_X1_PROBE_500V   | Voltage range from -500 V to 500 V   |
    | 15    | PICO_X1_PROBE_1KV    | Voltage range from -1000 V to 1000 V |
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
  - NOTE: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-B]:range` 

### OSCNAME:CH[A-D]:bandwidth
- **Type**: `mbbo`
- **Description**: The bandwith Oscilloscope start acquiring PV with current configuration(set by other PVs).
- **Fields**:
  - `VAL`: Trigger to start getting the waveform.
    | VAL   | Enum             | Description              |
    |-------|------------------|--------------------------|
    | 0     | PICO_BW_FULL     | Full bandwith (defualt)  |
    | 1     | PICO_BW_20MHZ    | Bandwith of 20 MHZ       |
    | 2     | PICO_BW_200MHZ   | Bandwith of 200 MHZ      |
- **Example**:
  ```bash
    # Set bandwith to full bandwith by number
    $ caput OSC1234-01:CHA:bandwith 0
    # Set bandwith to 20 MHZ bandwith by enum
    $ caput OSC1234-01:CHA:bandwith PICO_BW_20MHZ

    # Get bandwith
    $ caget OSC1234-01:CHA:bandwith
  ```

### OSCNAME:CH[A-D]:bandwith:fbk
- **Type**: `mbbi`
- **Description**: The actual value of the voltage range set to a channel.  
  - NOTE: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-B]:bandwith` 

### OSCNAME:CH[A-D]:analog_offset
- **Type**: `ao`
- **Description**: A voltage to add to the input channel before digitization.
- **Fields**:
  - `VAL`: The value of the voltage offset.
- **Example**:
  ```bash
    # Set offset to 1V
    $ caput OSC1234-01:CHA:analogoffset 1
    # Set offset to 10V
    $ caput OSC1234-01:CHA:analogoffset 10

    # Get offset
    $ caput OSC1234-01:CHA:analogoffset
  ```
### OSCNAME:CH[A-D]:analog_offset:fbk
- **Type**: `ai`
- **Description**: The actual voltage to added to the input channel before digitization. The analog offset voltage had limits which depend on the voltage range and coupling set to a channel. If the value put to `OSCNAME:CH[A-D]:analog_offset` is outside of the limits, the max or min value will be used and will be reported by this PV. 
  - NOTE: This value is only true when `OSCNAME:CH[A-D]:ON:fbk` reports ON. 
- **Fields**: 
  - `VAL`: See `OSCNAME:CH[A-B]:analog_offset` 

### OSCNAME:CH[A-D]:analog_offset:max 
- **Type**: `ai`
- **Description**: The maximun allowed analog offset voltage allowed for the range. 
  - Updated when the value of `OSCNAME:CH[A-B]:range` or `OSCNAME:CH[A-B]:coupling` are changed. 

### OSCNAME:CH[A-D]:analog_offset:min
- **Type**: `ai`
- **Description**: The minimum allowed analog offset voltage allowed for the range. 
  - Updated when the value of `OSCNAME:CH[A-B]:range` or `OSCNAME:CH[A-B]:coupling` are changed. 


### OSCNAME:CH[A-D]:waveform:start
- **Type**: `bo`
- **Description**: The PV to ask Oscilloscope start acquiring wavefrom with current configuration(set by other PVs).
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

### OSCNAME:CH[A-D]:waveform:stop
- **Type**: `bo`
- **Description**: The PV to ask Oscilloscope stop acquiring.
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

### OSCNAME:CH[A-D]:waveform
- **Type**: `waveform`
- **Description**: Waveform will be available after `OSCNAME:CH[A-D]:waveform:start` PV is invoked.
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
      
