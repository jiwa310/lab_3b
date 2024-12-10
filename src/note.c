#include "note.h"
#include <stdio.h>
#include "xil_cache.h"
#include <mb_interface.h>
#include "xparameters.h"
#include <xil_types.h>
#include <xil_assert.h>
#include <xio.h>
#include "xtmrctr.h"

#include <stddef.h>

//#include "lcd.h"

//array to store note names for findNote
static char notes[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

//finds and prints note of frequency and deviation from note
void findNote(float f) {
	float c=261.63;
	float r;
	int oct=4;
	int note=0;
	//determine which octave frequency is in
	if(f >= c) {
		while(f > c*2) {
			c=c*2;
			oct++;
		}
	}
	else { //f < C4
		while(f < c) {
			c=c/2;
			oct--;
		}
	
	}

	//find note below frequency
	//c=middle C
	r=c*root2;
	while(f > r) {
		c=c*root2;
		r=r*root2;
		note++;
	}

	//determine which note frequency is closest to
	if((f-c) <= (r-f)) { //closer to left note
		xil_printf(notes[note]);
		xil_printf("%d ", oct);
	}
	else { //f closer to right note
		note++;
		if(note >=12) note=0;
		xil_printf(notes[note]);
		xil_printf("%d ", oct);
	}

   /*
	//determine which note frequency is closest to
	if((f-c) <= (r-f)) { //closer to left note
		WriteString("N:");
		WriteString(notes[note]);
		WriteInt(oct);
		WriteString(" D:+");
		WriteInt((int)(f-c+.5));
		WriteString("Hz");
	}
	else { //f closer to right note
		note++;
		if(note >=12) note=0;
		WriteString("N:");
		WriteString(notes[note]);
		WriteInt(oct);
		WriteString(" D:-");
		WriteInt((int)(r-f+.5));
		WriteString("Hz");
	}
   */
}

void findNoteForDisplay(float f, char* noteStr, size_t size, float baseA4) {
    float c = 261.63f * (baseA4 / 440.0f);    // Scale C4 based on A4 tuning
    float r;
    int oct = 4;
    int note = 0;

    // Determine octave
    if(f >= c) {
        while(f > c*2) {
            c = c*2;
            oct++;
        }
    }
    else { //f < C4
        while(f < c) {
            c = c/2;
            oct--;
        }
    }

    // Find note below frequency
    r = c*root2;
    while(f > r) {
        c = c*root2;
        r = r*root2;
        note++;
    }

    // Determine closest note
    if((f-c) <= (r-f)) { // closer to left note
        snprintf(noteStr, size, "%s%d", notes[note], oct);
    }
    else { // closer to right note
        note++;
        if(note >= 12) note = 0;
        snprintf(noteStr, size, "%s%d", notes[note], oct);
    }
}
