# Impromptu Modular: Modules for [VCV Rack](https://vcvrack.com) by Marc Boulé

Version 0.6.3

[//]: # (!!!!!UPDATE VERSION NUMBER IN MAKEFILE ALSO!!!!!)

Available in the VCV Rack [plugin manager](https://vcvrack.com/plugins.html).


## License

Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics from the Component Library by Wes Milholen.

See ./LICENSE.txt for all licenses (and ./res/fonts/ for font licenses).


## Acknowledgements

Thanks to Alfredo Santamaria and Nay Seven for suggestions regarding improvements to Phrase-Seq-16. A very special thank you to Nigel Sixsmith for the many fruitful discussions and numerous design improvements that were suggested to Phrase-Seq-16, and also for the in-depth presentation of Phrase-Seq-16 and Twelve-Key in [Talking Rackheads episode 8](https://www.youtube.com/watch?v=KOpo2oUPTjg).


# Modules

* [Twelve-Key](#twelve-key): Chainable one octave keyboard controller.

* [Write-Seq-32](#write-seq-32): 32-step sequencer with CV in for easy sequence programming.

* [Write-Seq-64](#write-seq-64): 64-step sequencer with CV in for easy sequence programming.

* [Phrase-Seq-16](#phrase-seq-16): 16-phrase sequencer with 16 steps per sequence.

Details about each module are given below. Feedback and bug reports are always appreciated!



## Twelve-Key <a id="twelve-key"></a>

![IM](res/img/TwelveKey.jpg)

A chainable keyboard controller for your virtual Rack. When multiple Twelve-Key modules are connected in series from left to right, only the octave of the left-most module needs to be set, all other down-chain modules' octaves are set automatically. The aggregate output is that of the right-most module. To set up a chain of Twelve-Key modules, simply connect the three outputs on the right side of a module to the three inputs of the next module beside it (typically to the right).

Here are some specific details on each element on the faceplate of the module. For a brief tutorial on seting up the controller, please see [this segment](https://www.youtube.com/watch?v=KOpo2oUPTjg&t=874s) of Nigel Sixsmith's Talking Rackheads episode 8.

* **CV output**: Outputs the CV from the keyboard or its CV input, depending on which key was last pressed (an up-chain key or a key of the given keyboard module).

* **Gate output**: Gate signal from the keyboard or its gate input.

* **Octave +/-**: Buttons to set the base octave of the module. These buttons have no effect when a cable is connected to the Oct input.

* **Oct**: CV input to set the base octave of the module. The voltage range is 0V (octave 0) to 9V (octave 9). Non-integer voltages or voltages outside this range are floored/clamped. 

* **Oct+1**: CV output for setting the voltage of the next down-chain Twelve-Key module. This corresponds to the base octave of the current module incremented by 1V.



## Write-Seq-32 <a id="write-seq-32"></a>

![IM](res/img/WriteSeq32.jpg)

A three channel 32-step writable sequencer module. This sequencer was designed to allow the entering of notes into a sequencer in a quick and natural manner when using, for example:

* a midi keyboard connected via the Core MIDI-1 module in VCV Rack;
* a software midi keyboard such as [VMPK](http://vmpk.sourceforge.net/) (a software midi loopback app may be required); 
* a keyboard within Rack such as the Autodafe keyboard or [Twelve-Key](#twelve-key). 

Although the display shows note names (ex. C4#, D5, etc.), any voltage within the -10V to 10V range can be stored/played in the sequencer, whether it is used as a pitch CV or not, and whether it is quantized or not.

Ideas: send the midi keyboard's CV into the sequencer's CV in, and send the keyboard's gate signal into the sequencer's Write input. With autostep activated, each key-press will automatically be entered in sequence. Gate states and window selection can be done by pressing the 8 and 4 LED buttons respectively located below and above the main display. 

Here are some specific details on each element on the faceplate of the module. Familiarity with the Fundamental SEQ-3 sequencer is recommended, as some operating principles are similar in both sequencers.

* **Autostep**: Will automatically step the sequencer one step right on each write. No effect on channels 1 to 3 when the sequencer is running. An alternative way of automatically stepping the sequencer each time a note is entered is to send the gate signal of the keyboard to both the write and StepR inputs.

* **Window**: LED buttons to display the active 8-step window within the 32 step sequence (hence four windows). No effect on channels 1 to 3 when the sequencer is running.

* **Sharp / flat**: Determines whether to display notes corresponding to black keys using either the sharp or flat symbols used in music notation. See _Notes display_ below for more information.

* **Quantize**: Quantizes the CV IN input to a regular 12 semi-tone equal temperament scale. Since this quantizes the CV IN, some channels can have quantized CVs while others do not. 

* **Step LEDs**: Shows the current position of the sequencer in the given window.

* **Notes display**: Shows the note names for the 8 steps corresponding to the active window. When a stored pitch CV has not been quantized, the display shows the closest such note name. For example, 0.03 Volts is shown as C4, whereas 0.05 Volts is shown as C4 sharp or D4 flat. Octaves above 9 or below 0 are shown with a top bar and an underscore respectively.

* **Gates**: LED buttons to show/modify the gates for the 8 steps in the current window. See _Gate 1-3_ below for more information on gate signals. Gates can be toggled whether the sequencer is running or not.

* **Chan**: Selects the channel that is to be displayed/edited in the top part of the module. Even though this is a three channel sequencer, a fourth channel is available for staging a sequence while the sequencer is running (or not). 

* **Copy-Paste**: Copy and paste the CVs and gates of a channel into another channel. In a given channel, press the left button to copy the channel into a buffer, then select another channel and press the right button to paste. All 32 steps are copied irrespective of the STPES knob setting.

* **Paste sync**: Determines whether to paste in real time (RT), on the next clock (CLK), or at the start of the next sequence (SEQ). Pending pastes are shown by a red LED beside CLK/SEQ, and if the selected channel changes, the paste operation will be performed in the channel that was selected when the paste button was pressed. Pastes into the staging area (channel 4) are always done in real time, irrespective of the state of the paste sync switch. To cancel a pending paste, press the copy button again.

* **Step L/R**: These buttons step the sequencer one step left or right. No effect on channels 1 to 3 when the sequencer is running.

* **Run 1-3**: Start/stop the sequencer. When running, the sequencer responds to rising edges of the clock input and will step all channels except the staging area (channel 4). When Run is activated, the sequencer automatically starts playing at the first step.

* **Write**: This button is used to trigger the writing of the voltage on the CV IN jack into the CV of the current step of the selected channel. If a wire is connected to GATE IN, this gate input is also written into the gate of the current step/channel. An enabled gate corresponds to a voltage of 1V or higher. The small LED indicates if writing via the write button is possible (green) or not (red).

* **CV In**: This CV is written into the current step of the selected channel. Any voltage between -10V and 10V is supported. See _Notes display_ and _Quantize_ above for more related information. No effect on channels 1 to 3 when the sequencer is running.

* **Gate In**: Allows the gate of the current step/channel to also be written during a Write (see above). If no wire is connected, the input is ignored and the currently stored gate is unaffected. No effect on channels 1 to 3 when the sequencer is running.

* **Steps**: Sets the number of steps for all the sequences (sequence length).

* **Monitor**: This switch determines which CV will be routed to the currently selected channel's CV output when the sequencer is not running. When the switch is in the right position, the CV stored in the sequencer at that step is output; in the left position, the CV applied to the CV IN jack is output.

* **CV 1-3**: CV outputs of each channel at the current step.

* **Gate 1-3**: Gate for each channel at the current step. The duration of the gates corresponds to the high time of the clock signal.

* **Run input**: Control voltage for starting and stopping the sequencer. A rising edge triggered at 1V will toggle the run mode.

* **Write input**: Control voltage for writing CVs into the sequencer (Writebutton). A rising edge triggered at 1V will perform the write action (see _Write_ above).

* **Step L/R inputs**: Control voltages for step selection (Step L/R buttons). A rising edge triggered at 1V will step the sequencer left/right by one step.

* **Reset input**: Repositions the sequencer at the first step. A rising edge triggered at 1V will be detected as a reset. Pending pastes are also cleared.

* **Clock**: When the sequencer is running, each rising edge (1V threshold) will advance the sequencer by one step. The width (duration) of the high pulse of the clock is used as the width (duration) of the gate outputs. 



## Write-Seq-64 <a id="write-seq-64"></a>

![IM](res/img/WriteSeq64.jpg)

A four channel 64-step writable sequencer module. This sequencer is based on Write-Seq-32, both of which share many of the same functionalities. Write-Seq-64 has dual clock inputs (each controls a pair of channels). This sequencer is more versatile than Write-Seq-32 since each channel has its own step position and maximum number of steps. Sequences of different lengths can be created, with different starting points.

Ideas: The first part of the famous [Piano Phase](https://en.wikipedia.org/wiki/Piano_Phase) piece by Steve Reich can be easily programmed into the sequencer by entering the twelve notes into channel 1 with a keyboard, setting STEPS to 12, copy-pasting channel 1 into channel 3, and then driving each clock input with two LFOs that have ever so slightly different frequencies. Exercise left to the reader!

Here are some specific details on elements of the faceplate which differ compared to Write-Seq-32. Familiarity with Write-Seq-32 is strongly recommended.

* **Chan**: Four channels available, with a fifth channel that can be used as a staging area.

* **Gate LED and CV display**: Status of the gate and CV of the currently selected step.

* **Steps**: Sets the number of steps for the currently selected sequence (sequence length). Each channel can have different lengths. This value is included as part of a copy-paste operation.

* **Reset input/button**: Repositions all channels to their first step. A rising edge triggered at 1V will be detected as a reset. Pending pastes are also cleared.

* **Clock 1,2**: Clock signal for channels 1 and 2.

* **Clock 3,4**: Clock signal for channels 3 and 4. If no wire is connected, _Clock 1,2_ is used internally for channels 3 and 4.



## Phrase-Seq-16 <a id="phrase-seq-16"></a>

![IM](res/img/PhraseSeq16.jpg)

A 16 phrase sequencer module, where each phrase is an index into a set of 16 sequences of 16 steps (maximum). CVs can be entered via a CV input when using an external keyboard controller or via the built-in keyboard on the module itself.

Ideas: If you need a 256-step sequence in a single module, this is the sequencer for you!

The following block diagram shows how sequences and phrases relate to each other to create a song. In the diagram, a 12-bar blues pattern is created by setting the song length to 12, the step lengths to 8 (not visible in the figure), and then creating 4 sequences. The 12 phrases are indexes into the 4 sequences that were created. (Not sure anyone plays blues in a modular synth, but it shows the idea at least!)

![IM](res/img/PhraseSeq16BlockDiag.jpg)

Here are some specific details on elements of the faceplate. Familiarity with the Fundamental SEQ-3 sequencer is recommended, as some operating principles are similar in both sequencers. For an in depth review of the sequencer's capabilities, please see Nigel Sixsmith's [Talking Rackheads episode 8](https://www.youtube.com/watch?v=KOpo2oUPTjg).

* **Seq/Song**: This is the main switch that controls the two major modes of the sequencer. Seq mode allows the currently selected sequence to be played/edited. In this mode, all controls are available (run mode, transpose, rotate, copy-paste, gates, slide, octave, notes) and the content of a sequence can be modified even when the sequencer is running. Song mode allows the creation of a series of sequence numbers (called phrases). In this mode, only the run mode and the sequence index numbers themselves can be modified (whether the sequence is running or not); the other aforementioned controls are unavailable and the actual contents of the sequences cannot be modified.

* **Length**: When in SEQ mode, allows the arrow buttons to select the length of sequences (number of steps), the default is 16. All sequences have the same length. When in SONG mode, allows the arrow buttons to select the number of phrases in the song (the default is 4).

* **Left/right arrow buttons**: These buttons step the sequencer one step left or right. No effect when Attach is activated (see _Attch_ below).

* **Seq#**: In Seq mode, this number determines which sequence is being edited/played. In Song mode, this number determines the sequence index for the currently selected phrase; the selected phrase is shown in the 16 LEDs at the top of the module). When one of the Mode, Transpose, Rotate buttons is pressed, the display instead shows the current run mode (see Mode below), the amount of semi-tones to transpose, and the number of steps to rotate respectively. Clicking on any button returns the display to it normal setting that shows the sequence number.

* **Attach**: Allows the edit head to follow the run head (attach on). The position of the edit head is shown with a red LED, and the position of the run head is shown by a green LED. When in Seq mode, the actual content of a step (i.e. note, oct, gates, slide) of the sequence can be modified in real time as the sequencer is advancing (_attach_ on), or manually by using the < and > buttons (_attach_ off).

* **Mode**: This controls the run mode of the sequences and the song (one setting for each). The modes are: FWD (forward), REV (reverse), PPG (ping-pong, also called forward-reverse), BRN (brownian random), RND (random). For example, setting the run mode to FWD for Seq and to RND for Song will play the phrases that are part of a song randomly, and the probablility of a given phrase playing is proportional to the number of times it appears in the song.

* **Transpose**: Transpose the currently selected sequence up or down by a given number of semi-tones. The main knob is used to set the transposition amount. Only available in Seq mode.

* **Rotate**: Rotate the currently selected sequence left or right by a given number of steps. The main knob is used to set the rotation amount. Only available in Seq mode.

* **Copy-Paste**: Copy and paste the CVs, gates and slide states of a sequence into another sequence. Press the left button to copy the channel into a buffer, then select another sequence and press the right button to paste. All 16 steps are copied irrespective of the length of the sequences. Only available in Seq mode.

* **Oct and keyboard**: When in Seq mode, the octave LED buttons and the keyboard can be used to set the notes of a sequence. The octave and keyboard LEDs are used for display purposes only in Song mode.

* **Gate 1, 2 buttons and probability knob**: The gate buttons control whether the gate of a current step is active or not. The probability knob controls the chance that when gate 1 is active it is actually sent to its output jack. In the leftmost position, no gates are output, and in the rightmost position, gates are output exactly as stored in a sequence. This knob's probability setting is not memorized for each step and applies to the sequencer as a whole.

* **Slide**: Portamento between CVs of successive steps. Slide can be activated for a given step using the slide button. The slide duration can be set using the small knob below the slide button (0 to 2 seconds, default 150ms). This knob's setting is not memorized for each step and applies to the sequencer as a whole.

* **Tie Step**: When CVs are intended to be held across subsequent steps, this button can be used to more easily copy the CV of the previous step into the current step, and also to automatically turn off the gates of the current step. This is in essence a shorcut (or a macro) to save time and its action is immediate, i.e. no additional state information is saved in the sequencer regarding tied steps.

* **CV**: CV output of the sequence/song the current step.

* **Gate 1, 2 output**: Gate signal outputs for each channel at the current step. The duration of the gates corresponds to the high time of the clock signal. Gates can be turned on/off using the Gate buttons. Gate 2 is perfect for using as an accent if desired.

* **Write input**: Control voltage for triggering the writing of a CV into the sequencer via the CV in jack (see below). A rising edge triggered at 1V will perform the write action.

* **CV In**: This CV is written into the current step of the selected sequence. Any voltage between -10V and 10V is supported. When a CV is not quantized, the closest key is illuminated on the keyboard; octaves greater than 7 or smaller than 1 are not displayed by the octave LEDs.

* **Autostep**: Will automatically step the sequencer one step right on each write. This works with the Write input only, and has no effect when entering notes with the onboard keys.

* **Run input**: Control voltage for starting and stopping the sequencer. A rising edge triggered at 1V will toggle the run mode.

* **Seq# input**: Control voltage used to select the active sequence (Seq mode only). A 0 to 10V input is proportionnaly mapped to the 1 to 16 sequence numbers. This can be used to externally control the playing order of the sequences.

* **Mode input**: Control voltage used to select the run mode of sequences. A 0 to 10V input is proportionnaly mapped to the 5 run modes (see _Mode_ above).

* **Left/right arrow inputs**: Control voltages for step/phrase selection. A rising edge triggered at 1V will step the position in the sequence or song left/right by one step.

* **Reset input/button**: Repositions the run and edit heads of the sequence or song to the first step. A rising edge triggered at 1.0V will be detected as a reset.

* **Clock**: When the sequencer is running, each rising edge (1V threshold) will advance the sequencer by one step. The width (duration) of the high pulse of the clock is used as the width (duration) of the gate outputs. 
