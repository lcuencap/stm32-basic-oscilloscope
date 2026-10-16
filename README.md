# STM32 Basic Oscilloscope

A university embedded-systems project implementing a **basic two-channel oscilloscope** on an STM32F429 development board.

The system acquires two analog sensor signals using ADC and DMA, synchronizes sampling with encoder events, processes the captured data and displays both waveforms on the board's TFT screen.

```text
Two analog inputs
       |
       v
   ADC1 + ADC3
       |
       v
      DMA
       |
       v
 Sample buffers
       |
       v
 Signal processing
       |
       v
 Two waveform views
       |
       v
   TFT display
```

## What the system does

The project combines several microcontroller peripherals into one acquisition and visualization pipeline:

- two analog input channels;
- ADC acquisition;
- DMA-based block transfers;
- event-based acquisition synchronized with encoder edges;
- timing measurements between acquisition triggers;
- conversion from raw ADC samples to physical measurements;
- min, max and average statistics;
- downsampling / smoothing for display;
- two waveform plots rendered on the STM32 TFT;
- application-state management;
- software simulation for selected encoder inputs.

The result is not intended to replace a laboratory oscilloscope. It is a purpose-built embedded oscilloscope for the acquisition scenario defined by the university project.

## Acquisition flow

```text
Encoder index
     |
     v
Wait for first valid A/B edge
     |
     v
Lock acquisition edge pattern
     |
     v
ADC + DMA capture
     |
     v
Complete sample buffers
     |
     v
Process both channels
     |
     v
Refresh TFT waveforms
```

The acquisition logic tracks matching encoder edges and uses double-buffered DMA staging before committing data to the complete signal buffers.

## Signal processing

Raw ADC values are converted to voltage and then to the physical sensor measurement used by the project.

For each channel, the processing stage calculates:

- average value;
- minimum value;
- maximum value;
- a reduced set of samples for visualization.

A small moving window is used when generating the processed waveform displayed on screen.

## Display

The TFT is driven through the STM32 LTDC peripheral.

Two logical waveform windows are rendered:

- front sensor waveform;
- rear sensor waveform.

The implementation uses two framebuffer layers:

- a static layer for frames and labels;
- a transparent dynamic layer refreshed with the current waveforms.

## Main modules

```text
Core/Src/main.c
    Application state machine and coordination

Core/Src/sensor_capture.c
    ADC/DMA acquisition and trigger synchronization

Core/Src/sensor_process.c
    Signal conversion, statistics and display samples

Core/Src/display.c
    Waveform rendering and framebuffer access

Core/Src/encoder.c
    Encoder decoding, direction and speed measurement

Core/Src/encoder_simulator.c
    Software simulation for development/testing

Core/Src/timers.c
    Application timing support
```

The peripheral initialization generated for ADC, DMA, timers, LTDC, FMC, GPIO and SPI is also retained under `Core/`.

## Hardware / development environment

The original project was developed for an **STM32F429** board with TFT display using **STM32CubeIDE / STM32CubeMX**.

`Practica3.ioc` contains the CubeMX peripheral configuration.

This curated portfolio repository focuses on the application and project-specific source code rather than duplicating the complete STM32 HAL/CMSIS vendor libraries. Those dependencies can be regenerated or provided by STM32CubeIDE from the project configuration.

Reproducing the complete physical acquisition setup requires compatible hardware and the corresponding analog sensors / encoder signals.

## Authorship

This was a **university group project** and is presented as shared academic work. The repository does not claim exclusive authorship of individual modules.

## Context

Developed during the Computer Engineering degree at La Salle - Ramon Llull University.

This repository is a curated portfolio version of the original academic project. The original development history remains private.
