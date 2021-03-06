//
//  LabMidiSong.cpp
//

/*
 Copyright (c) 2012, Nick Porcino
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * The names of its contributors may not be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// The parser is loosely ported to C++ from jasmid/midifile.js obtained from github 12/11/17
// https://github.com/gasman/jasmid
//
// Thanks to the jasmid team for posting such clean and useful code on github.
//
// This is the only jasmid ported source in this library. Jasmid carried the
// following license when obtained from github:
/*
Copyright (c) 2010, Matt Westcott & Ben Firshman
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* The names of its contributors may not be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// The Base64 decoder is based on
//  http://base64.sourceforge.net/b64.c
// It carries the copyright notice:
/*
 MODULE NAME:    b64.c

 AUTHOR:         Bob Trower 08/04/01

 PROJECT:        Crypt Data Packaging

 COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

 NOTE:           This source code may be used as you wish, subject to
 the MIT license.  See the LICENCE section below.

 LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the
 Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall
 be included in all copies or substantial portions of the
 Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "LabMidiSong.h"

#include "LabMidiCommand.h"

#include <iostream>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <vector>

using namespace std;

namespace Lab {

    /* read a MIDI-style variable-length integer
     (big-endian value in groups of 7 bits,
     with top bit set to signify that another byte follows)
     */
	int readVarInt(uint8_t const*& dataStart)
    {
        int result = 0;
		while (true) {
			uint8_t b = *dataStart++;
			if (b & 0x80) {
				result += (b & 0x7f);
				result <<= 7;
			} else {
				return result + b; // b is the last byte
			}
		}
	}


    int readInt16(uint8_t const*& dataStart)
    {
        int result = int(*dataStart++) << 8;
        result += int(*dataStart++);
        return result;
    }

    int readInt24(uint8_t const*& dataStart)
    {
        int result = int(*dataStart++) << 16;
        result += int(*dataStart++) << 8;
        result += int(*dataStart++);
        return result;
    }

    int readInt32(uint8_t const*& dataStart)
    {
        int result = int(*dataStart++) << 24;
        result += int(*dataStart++) << 16;
        result += int(*dataStart++) << 8;
        result += int(*dataStart++);
        return result;
    }


    MidiEvent* parseEvent(uint8_t const*& dataStart, uint8_t lastEventTypeByte)
    {
        uint8_t eventTypeByte = *dataStart++;

		if ((eventTypeByte & 0xf0) == 0xf0) {
			/* system / meta event */
			if (eventTypeByte == 0xff) {
				/* meta event */
                uint8_t subtypeByte = *dataStart++;
				int length = readVarInt(dataStart);
				switch(subtypeByte) {
					case 0x00: {
						if (length != 2) throw "Expected length for sequenceNumber event is 2";
                        SequenceNumberEvent* event = new SequenceNumberEvent();
						event->number = * (uint16_t*) dataStart;
                        dataStart += 2;
						return event;
                    }
					case 0x01: {
                        TextEvent* event = new TextEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x02: {
                        CopyrightNoticeEvent* event = new CopyrightNoticeEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x03: {
                        TrackNameEvent* event = new TrackNameEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x04: {
                        InstrumentNameEvent* event = new InstrumentNameEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x05: {
                        LyricsEvent* event = new LyricsEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x06: {
                        MarkerEvent* event = new MarkerEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x07: {
                        CuePointEvent* event = new CuePointEvent();
						event->text.assign((char*) dataStart, length);
                        dataStart += length;
						return event;
                    }
					case 0x20: {
						if (length != 1) throw "Expected length for midiChannelPrefix event is 1";
                        MidiChannelPrefixEvent* event = new MidiChannelPrefixEvent();
						event->channel = *(uint8_t*) dataStart;
                        ++dataStart;
						return event;
                    }
					case 0x2f: {
						if (length != 0) throw "Expected length for endOfTrack event is 0";
                        EndOfTrackEvent* event = new EndOfTrackEvent();
						return event;
                    }
					case 0x51: {
						if (length != 3) throw "Expected length for setTempo event is 3";
                        SetTempoEvent* event = new SetTempoEvent();
						event->microsecondsPerBeat = readInt24(dataStart);
						return event;
                    }
					case 0x54: {
						if (length != 5) throw "Expected length for smpteOffset event is 5";
                        SmpteOffsetEvent* event = new SmpteOffsetEvent();
						uint8_t hourByte = *dataStart++;
                        switch (hourByte & 0x60) {
                            case 0x00: event->framerate = 24; break;
                            case 0x20: event->framerate = 25; break;
                            case 0x40: event->framerate = 29; break;
                            case 0x60: event->framerate = 30; break;
                        }
						event->hour = hourByte & 0x1f;
						event->min = int(*dataStart++);
						event->sec = int(*dataStart++);
						event->frame = int(*dataStart++);
						event->subframe = int(*dataStart++);
						return event;
                    }
					case 0x58: {
						if (length != 4) throw "Expected length for timeSignature event is 4";
						TimeSignatureEvent* event = new TimeSignatureEvent();
						event->numerator = int(*dataStart++);
						event->denominator = int(powf(2.0f, float(*dataStart++)));
						event->metronome = int(*dataStart++);
						event->thirtyseconds = int(*dataStart++);
						return event;
                    }
					case 0x59: {
						if (length != 2) throw "Expected length for keySignature event is 2";
                        KeySignatureEvent* event = new KeySignatureEvent();
						event->key = int(*dataStart++);
						event->scale = int(*dataStart++);
						return event;
                    }
					case 0x7f: {
                        SequencerSpecificEvent* event = new SequencerSpecificEvent();
                        event->data = new uint8_t[length];
                        memcpy(event->data, dataStart, length);
                        dataStart += length;
						return event;
                    }
				}
                // console.log("Unrecognised meta event subtype: " + subtypeByte);
                UnknownEvent* event = new UnknownEvent();
                event->data = new uint8_t[length];
                memcpy(event->data, dataStart, length);
                dataStart += length;
                return event;
			}
            else if (eventTypeByte == 0xf0) {
                int length = readVarInt(dataStart);
                SysExEvent* event = new SysExEvent();
                event->data = new uint8_t[length];
                memcpy(event->data, dataStart, length);
                dataStart += length;
                return event;
			}
			else if (eventTypeByte == 0xf7) {
                int length = readVarInt(dataStart);
                DividedSysExEvent* event = new DividedSysExEvent();
                event->data = new uint8_t[length];
                memcpy(event->data, dataStart, length);
                dataStart += length;
                return event;
			}
            else {
				throw "Unrecognised MIDI event type byte"; // eventTypeByte;
			}
		}
        else {
			/* channel event */
            ChannelEvent* event = new ChannelEvent();
			int param1;
			if ((eventTypeByte & 0x80) == 0) {
                // Running status is described here:
                // http://home.roadrunner.com/~jgglatt/tech/midispec/run.htm
				// running status - reuse lastEventTypeByte as the event type.
                // eventTypeByte is actually the first parameter
                //
				param1 = eventTypeByte;
				eventTypeByte = lastEventTypeByte;
			}
            else {
				param1 = int(*dataStart++);
				lastEventTypeByte = eventTypeByte;
			}
            event->midiCommand = eventTypeByte;
            event->param1 = param1;
            event->param2 = 0xff;   // don't transmit this value

			switch (eventTypeByte & 0xf0) {
				case 0x80:  // note off
                    event->param2 = int(*dataStart++);
					return event;
				case 0x90: // note on
                    event->param2 = int(*dataStart++); // velocity
					return event;
				case 0xa0: // after touch
                    event->param2 = int(*dataStart++); // amount
					return event;
				case 0xb0: // controller
                    event->param2 = int(*dataStart++); // amount
					return event;
				case 0xc0: // program change
					return event;
				case 0xd0: // channel after touch
					return event;
				case 0xe0: // pitch bend
                    event->param2 = int(*dataStart++);
					return event;
				default:
					throw "Unrecognised MIDI event type";
			}
		}
        throw "Unparsed event";
	}

    MidiSong::MidiSong()
    : tracks(0)
    , ticksPerBeat(240)     // precision (number of ticks distinguishable per second)
    , startingTempo(120)
    {
    }

    MidiSong::~MidiSong()
    {
        clearTracks();
    }

    void MidiSong::clearTracks()
    {
        if (tracks) {
            for (auto i = tracks->begin(); i != tracks->end(); ++i)
                delete *i;
            delete tracks;
        }
    }


    /*
     ** decodeblock
     **
     ** decode 4 '6-bit' characters into 3 8-bit binary bytes
     */
    void decodeblock( unsigned char in[4], unsigned char out[3] )
    {
        out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
        out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
        out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
    }

    /*
     ** Translation Table to decode (created by author)
     */
    static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

    /*
     ** decode
     **
     ** decode a base64 encoded stream discarding padding, line breaks and noise
     */
    void decode64(uint8_t const* infile, uint8_t* outfile, int filelen)
    {
        unsigned char in[4], out[3], v;
        int i, len;

        while(filelen > 0) {
            for( len = 0, i = 0; i < 4 && (filelen > 0); i++ ) {
                v = 0;
                while( (filelen > 0) && v == 0 ) {
                    v = (unsigned char) *infile++;
                    --filelen;
                    v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
                    if( v ) {
                        v = (unsigned char) ((v == '$') ? 0 : v - 61);
                    }
                }
                if(filelen > 0) {
                    len++;
                    if( v ) {
                        in[ i ] = (unsigned char) (v - 1);
                    }
                }
                else {
                    in[i] = 0;
                }
            }
            if( len ) {
                decodeblock( in, out );
                for( i = 0; i < len - 1; i++ ) {
                    *outfile = out[i];
                    ++outfile;
                }
            }
        }
    }


    void MidiSong::parse(uint8_t const*const a, int length, bool verbose)
    {
        uint8_t const* file = a;
        uint8_t* b = 0;

        // Check if the MIDI file has been base64 encoded using the scheme
        // Euphony has in its tracks files.
        // https://github.com/qiao/euphony
        //
        const char* base64Test = "data:audio/midi;base64,";
        int base64TestLen = strlen(base64Test);
        if (!strncmp((const char*) file, base64Test, base64TestLen)) {
            b = new uint8_t[length];
            decode64(a + base64TestLen, b, length - base64TestLen);
            file = b;
        }

        clearTracks();
        tracks = new std::vector<MidiTrack*>();

        uint8_t const* dataStart = file;

        int headerId = readInt32(dataStart);
        int headerLength = readInt32(dataStart);
        if (headerId != 'MThd' || headerLength != 6) {
            if (verbose)
                std::cerr << "Bad .mid file - couldn't parse header" << std::endl;
            return;
        }

        /*int formatType = */ readInt16(dataStart);
        int trackCount = readInt16(dataStart);
        int timeDivision = readInt16(dataStart);

        // CBB: deal with the SMPTE style time coding
        // timeDivision is described here http://www.sonicspot.com/guide/midifiles.html
        if (timeDivision & 0x8000) {
            if (verbose)
                std::cerr << "Found SMPTE time frames" << std::endl;
            //int fps = (timeDivision >> 16) & 0x7f;
            //int ticksPerFrame = timeDivision & 0xff;
            // given beats per second, timeDivision should be derivable.
            return;
        }

        startingTempo = 120.0f;
        ticksPerBeat = float(timeDivision); // ticks per beat (a beat is defined as a quarter note)
                                            // commonly 48 to 960.

        try {
            for (int i = 0; i < trackCount; ++i) {
                headerId = readInt32(dataStart);
                headerLength = readInt32(dataStart);
                if (headerId != 'MTrk') {
                    if (verbose)
                        std::cerr << "Bad .mid file - couldn't find track" << std::endl;
                    return;
                }

                tracks->push_back(new MidiTrack());
                MidiTrack* track = tracks->back();
                uint8_t const* dataEnd = dataStart + headerLength;
                uint8_t runningEvent = 0;
                while (dataStart < dataEnd) {
                    int duration = readVarInt(dataStart);
                    MidiEvent* ev = parseEvent(dataStart, runningEvent);
                    ev->deltatime = duration;
                    ChannelEvent* ce = dynamic_cast<ChannelEvent*>(ev);
                    if (ce)
                        runningEvent = ce->midiCommand;
                    track->events.push_back(ev);
                }
            }
        }
        catch(...)
        {
        }

        delete [] b;
    }

    void MidiSong::parse(char const*const path, bool verbose)
    {
        cout<<"哈哈"<<endl;
        FILE* f = fopen(path, "rb");
        if (f) {
            cout<<"呵呵"<<endl;
            fseek(f, 0, SEEK_END);
            int l = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t* a = new uint8_t[l];
            fread(a, 1, l, f);
            fclose(f);

            parse(a, l, verbose);
            delete [] a;
        }
    }

    //------------------------------------------------------------
    // MML support
    //

    int secondsToTicks(double seconds, double bpm, int ticksPerBeat)
    {
        double beats = seconds * (bpm / 60.0);
        double ticks = beats * ticksPerBeat;
        return int(ticks);
    }

    int wholeNoteToTicks(double fraction, double bpm, int ticksPerBeat)
    {
        double seconds = (bpm / 60.0f) / fraction;
        return secondsToTicks(seconds, bpm, ticksPerBeat);
    }

    int getMMLInt(char const*& curr)
    {
        int v = 0;
        char c = *curr;
        int sign = 1;
        while (c == '-' || (c >= '0' && c <= '9')) {
            if (c == '-')
                sign = -1;
            else
                v = v * 10 + (c - '0');
            ++curr;
            c = *curr;
        }
        return v * sign;
    }

    int sharpFlat(char const*& midifiledata)
    {
        char c = *midifiledata;
        if (c == '-') {
            ++midifiledata;
            return -1;
        }
        if (c == '+' || c == '#') {
            ++midifiledata;
            return 1;
        }
        return 0;
    }

    int dotted(char const*& midifiledata, int bpm, int ticksPerBeat, int l)
    {
        int ret = l;

        // denominator is a fraction of a whole note
        int denominator = getMMLInt(midifiledata);
        if (denominator)
            ret = denominator;

        int l2 = ret * 2;
        char c = *midifiledata;
        while (c == '.') {
            ++midifiledata;
            ret += l2;
            l2 = l2 * 2;
        }
        return wholeNoteToTicks(ret, bpm, ticksPerBeat);
    }

    // duration is in ticks
    //
    void storeMMLEvent(MidiTrack* track, uint8_t eventTypeByte, uint8_t note, uint8_t amount, int duration)
    {
        /* channel event */
        ChannelEvent* event = new ChannelEvent();
        event->deltatime = duration;
        event->midiCommand = eventTypeByte;
        event->param1 = note;
        event->param2 = amount;
        track->events.push_back(event);
    }

    // The variant of MML parsed here was produced by studying http://www.g200kg.com/en/docs/webmodular/,
    // mml2mid by Arle (unfortunately Arle's pages and the mml2mid sources are no longer online) and
    // the wikipedia artile http://en.wikipedia.org/wiki/Music_Macro_Language
    //

    // sample MML from http://www.g200kg.com/en/docs/webmodular/
    // t150 e-d-<g-4>g-rg-4e-d-<g-4>g-rg-4e-d-<g-4>g-4<e-4>g-4<d-4>frf4e-d-<d-4>frf4e-d-<d-4>frf4e-d-<d-4>f4<e-4>f4<g-4>g-rg-4

    void MidiSong::parseMML(char const*const mmlStr, int length, bool verbose)
    {
        char const* curr = mmlStr;
        char const* end = mmlStr + length;

        int octave = 4;
        int tr = 0;
        int err = 0;
        bool tied = false;
        int tempo = 120;
        int len = 8;        // an 1/8th note

        clearTracks();
        tracks = new std::vector<MidiTrack*>();
        tracks->push_back(new MidiTrack());
        MidiTrack* track = tracks->back();

        do {
            char c = *curr++;

            switch(c) {
            case 'l':   // length NN
            case 'L':
                len = getMMLInt(curr);
                break;

            case 'o':
            case 'O':
                octave = getMMLInt(curr);
                if (octave < 0)
                    octave = 0;
                if (octave > 7)
                    octave = 7;
                break;

            case '<':
                ++octave;
                if (octave > 7)
                    octave = 7;
                break;
            case '>':
                --octave;
                if (octave < 0)
                    octave = 0;
                break;

            case '@': { // tone selection
                int i = getMMLInt(curr);
                if (i < 0)
                    i = 0;
                if (i > 127)
                    i = 127;
                storeMMLEvent(track, MIDI_PROGRAM_CHANGE | tr, i, 0xff, 0);
                break;
            }

            case 't':       // tempo in bpm
            case 'T': {
                tempo = getMMLInt(curr);
                if (tempo < 0)
                    tempo = 0;
                if (tempo > 500)
                    tempo = 500;
                SetTempoEvent* event = new SetTempoEvent();
                event->microsecondsPerBeat = 60000000 / tempo;
                event->deltatime = 0;
                track->events.push_back(event);
                break;
            }

            case '/':
                tr++;
                if (tr > 15)
                    tr = 15;
                while (tr >= tracks->size())
                    tracks->push_back(new MidiTrack());
                track = (*tracks)[tr];
                break;

            case 'c':
            case 'C': {
                int note = 0 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'd':
            case 'D': {
                int note = 2 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'e':
            case 'E': {
                int note = 4 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'f':
            case 'F': {
                int note = 5 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'g':
            case 'G': {
                int note = 7 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'a':
            case 'A': {
                int note = 9 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }
            case 'b':
            case 'B': {
                int note = 11 + sharpFlat(curr) + octave * 12;
                int duration = dotted(curr, tempo, ticksPerBeat, len);
                // note output { tr, cnt, size 3, 0x90|tr note & 0x7f vol=0x7f }
                if (!tied) {
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0, duration);
                }
                else {
                    // note output { tr, cnt-1, size 3, 0x90|tr note & 0x7f vol=0x00 }
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, 0);
                    storeMMLEvent(track, MIDI_NOTE_ON | tr, note & 0x7f, 0x7f, duration);
                    tied = false;
                }
                break;
            }

            case '&': // tie (how to handle?)
                tied = true;
                break;

            case 'r': // rest
            case 'R':
                storeMMLEvent(track, MIDI_NOTE_ON | tr, 0, 0, dotted(curr, tempo, ticksPerBeat, len));
                tied = false;
                break;

            case ' ': // ignore
            case '	':
            case 10:
            case 13:
                break;

            default:
                err = 1;
                break;
            }
        } while (curr < end);
    }

    void MidiSong::parseMML(char const*const path, bool verbose)
    {
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            int l = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* a = new char[l];
            fread(a, 1, l, f);
            fclose(f);

            parseMML(a, l, verbose);
            delete [] a;
        }
    }

} // Lab
