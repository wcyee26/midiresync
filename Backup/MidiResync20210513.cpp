// MidiResync.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "MidiFile.h"
#include "Options.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

using namespace std;
using namespace smf;

// The input midi file
MidiFile iMidifile;
// The output midi file
MidiFile oMidifile;
// The margin time between same beats
float sameBeatMarginTime = 0.001;
// The minimum beat key number
int MIN_BEAT_KEY_NUMBER = 60;

string iMidifilename = "Nirvana About A Girl Unplugged Cover.mid";
//string iMidifilename = "Slipknot Psychosocial Cover.mid";

void outputMIDIFile();
void outputLog();
void outputLogFile();
bool isPreviousSameBeat(MidiEvent beatEvent, MidiEvent prevBeatEvent);
bool isNextSameBeat(MidiEvent beatEvent, MidiEvent nextBeatEvent);

int main(int argc, char** argv) {
    Options options;
    options.process(argc, argv);
    //MidiFile iMidifile;
    /*if (options.getArgCount() == 0) iMidifile.read(cin);
    else iMidifile.read(options.getArg(1));*/
    iMidifile.read(iMidifilename);
    iMidifile.doTimeAnalysis();
    iMidifile.linkNotePairs();        

    // The track count of input midi file
    int trackCount = iMidifile.getTrackCount();
    //int trackCount = 4;

    // The number of seconds per quarter note
    float tempoSeconds = 0.00;

    // Set TPQ for output midi file
    oMidifile.setTicksPerQuarterNote(iMidifile.getTicksPerQuarterNote());
    // Set tracks
    oMidifile.addTracks(trackCount - 1);

    // Set the maximum animation time for one beat in each track
    vector<float> maxAnimTimeLengths;
    //maxAnimTimeLengths.assign(trackCount, 1.0);
    maxAnimTimeLengths.assign(trackCount, 0.8);
    //maxAnimTimeLengths.assign(trackCount, 0.6);
    //maxAnimTimeLengths.assign(trackCount, 0.2);
    // Set the animation delta time between the start and the beat in each track
    vector<float> beforeHitAnimDTs;
    //beforeHitAnimDTs.assign(trackCount, 0.3666666666666667);
    beforeHitAnimDTs.assign(trackCount, 0.2933333333333333);
    //beforeHitAnimDTs.assign(trackCount, 0.22);
    //beforeHitAnimDTs.assign(trackCount, 0.0733333333333333);
    // Track 6 - Large Cymbal
    maxAnimTimeLengths[6] = 0.9;
    beforeHitAnimDTs[6] = 0.33;
    // Track 8 - Tai Lai Gong
    maxAnimTimeLengths[8] = 2.5;
    beforeHitAnimDTs[8] = 1.4;
    // Track 11 - Temple Block
    maxAnimTimeLengths[11] = 1.0;
    beforeHitAnimDTs[11] = 0.367;

    // Set the maximum signature animation time for each track
    vector<float> maxSignatureAnimTimeLengths;
    maxSignatureAnimTimeLengths.assign(trackCount, 0.0);
    // Track 2 - Jiangu
    maxSignatureAnimTimeLengths[2] = 4.0;
    // Track 3 - Lion Drum
    maxSignatureAnimTimeLengths[3] = 4.5;
    // Track 6 - Large Cymbal
    maxSignatureAnimTimeLengths[6] = 5.5;
    // Track 9 - Knobbed Gong
    maxSignatureAnimTimeLengths[9] = 1.5;

    for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {    
         
        // Number of beater uses for hitting the drum
        int beaterCount = 1;
        // The maximum animation time for one beat
        float maxAnimTimeLength = maxAnimTimeLengths[trackIndex];
        // The animation delta time between the start and the beat
        float beforeHitAnimDT = beforeHitAnimDTs[trackIndex];
        // The animation delta time between the beat and the end
        float afterHitAnimDT = maxAnimTimeLength - beforeHitAnimDT;
        // Comparing before hit anim delta time and after hit anim delta time
        float animTimeLengthRatio = beforeHitAnimDT / afterHitAnimDT;

        float aTime = 0.0;
        float bOnTime = 0.0;
        float bOffTime = 0.0;
        float cTime = 0.0;

        float adjustedBOnTime = 0.0;
        float adjustedBOffTime = 0.0;
        float delayedBOnTime = 0.0;
        float delayedBOffTime = 0.0;

        cout << "Track: " << trackIndex << endl;

        for (int eventIndex = 0; eventIndex < iMidifile[trackIndex].getEventCount(); eventIndex++) {
        //for (int eventIndex = 0; eventIndex < 201; eventIndex++) {
            // If the event is not a note
            if (!iMidifile[trackIndex][eventIndex].isNote()) {
                // Search for the number of beater in track name
                // Then assign it to varialbe 'beaterCount'
                if (iMidifile[trackIndex][eventIndex].isTrackName()) {
                    string content = iMidifile[trackIndex][eventIndex].getMetaContent();
                    size_t beaterCountSeperatorPos = content.find(':');
                    if (beaterCountSeperatorPos != string::npos)
                        beaterCount = stoi(content.substr(0, beaterCountSeperatorPos));
                        //cout << content << "beater count: " << beaterCount << endl;
                }
                // If beaterCount is 0 means the track is other than beat track. Eg. Character Anim, Lighting, etc.
                // So, copy and paste the track and break the event loop
                if (beaterCount == 0) {
                    oMidifile[trackIndex] = iMidifile[trackIndex];
                    break;
                }
                // Get the tempo seconds
                if (iMidifile[trackIndex][eventIndex].tick == 0
                    && iMidifile[trackIndex][eventIndex].getTempoSeconds() > 0) {
                    tempoSeconds = iMidifile[trackIndex][eventIndex].getTempoSeconds();
                    cout << "Tempo Seconds: " << tempoSeconds << endl;
                }
                oMidifile.addEvent(iMidifile[trackIndex][eventIndex]);
                continue;
            }

            if (iMidifile[trackIndex][eventIndex].isNoteOn()) {
                // If the previous beat is the SAME, SKIP ALL
                if (isPreviousSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex - 1])) {
                    continue;
                }
                // The beat hitting time
                float eventTime = iMidifile[trackIndex][eventIndex].seconds;

                float totalAvailableHitAnimDT = 0.0;
                float availableBeforeHitAnimDT = 0.0;
                float availableAfterHitAnimDT = 0.0;

                // The standard maximum time before the end of the beat
                cTime = iMidifile[trackIndex][eventIndex + 2].seconds;
                // If the next beat is SAME, 
                // Then the max. time before the end of the beat should be the next 4th event
                if (isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])) {
                    cTime = iMidifile[trackIndex][eventIndex + 4].seconds;
                }
                // If beaterCount is 1 OR the next beat is SAME, reduce the maximum time before the end of beat
                // and enter the FIRST LAYER of beat duration calculation
                if (beaterCount == 1 
                    || isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])
                    ) {
                    // Set the available hit anim delta time to ONLY between two ON notes
                    totalAvailableHitAnimDT = cTime - eventTime;
                    availableAfterHitAnimDT = (totalAvailableHitAnimDT / maxAnimTimeLength) * afterHitAnimDT;
                    cTime = eventTime + availableAfterHitAnimDT;
                }

                // SECOND LAYER of beat duration calculation
                // To analyse and optimize each beat more precisely
                availableBeforeHitAnimDT = eventTime - aTime;
                availableAfterHitAnimDT = cTime - eventTime;
                totalAvailableHitAnimDT = availableBeforeHitAnimDT + availableAfterHitAnimDT;
                // Comparing the available anim time length TO maximum anim time length
                float availableAnimTimeLengthRatio = totalAvailableHitAnimDT / maxAnimTimeLength;
                // If the available anim time length exceeds the maximum, override it to the maximum
                if (availableAnimTimeLengthRatio >= 1.0)
                    availableAnimTimeLengthRatio = 1.0;

                float desiredBeforeHitAnimDT = availableAnimTimeLengthRatio * beforeHitAnimDT;
                float desiredAfterHitAnimDT = availableAnimTimeLengthRatio * afterHitAnimDT;

                float allowedBeforeHitAnimDT = desiredBeforeHitAnimDT;
                float allowedAfterHitAnimDT = desiredAfterHitAnimDT;

                if (desiredAfterHitAnimDT > availableAfterHitAnimDT) {
                    allowedAfterHitAnimDT = availableAfterHitAnimDT;
                    allowedBeforeHitAnimDT = animTimeLengthRatio * allowedAfterHitAnimDT;
                }
                if (allowedBeforeHitAnimDT > availableBeforeHitAnimDT) {
                    allowedBeforeHitAnimDT = availableBeforeHitAnimDT;
                    allowedAfterHitAnimDT = allowedBeforeHitAnimDT / animTimeLengthRatio;
                }

                int keyNumber = iMidifile[trackIndex][eventIndex].getKeyNumber();
                int velocity = iMidifile[trackIndex][eventIndex].getVelocity();
                bOnTime = eventTime - allowedBeforeHitAnimDT;
                // If the current bOnTime is less than prev bOffTime
                // Means notes are overlapping, change the key number
                if (bOnTime < bOffTime) {
                    int prevKeyNumber = oMidifile[trackIndex][eventIndex - 2].getKeyNumber();
                    if (prevKeyNumber >= MIN_BEAT_KEY_NUMBER
                        && (abs(keyNumber - prevKeyNumber) % 2 == 0))
                        keyNumber = keyNumber + 1;
                }
                bOffTime = eventTime + allowedAfterHitAnimDT;
                float bAnimTimeLength = bOffTime - bOnTime;

                // ADJUST each beat to one event earlier, according to the previous note
                adjustedBOnTime = bOnTime - bAnimTimeLength;
                // If adjustedBOnTime less than prev adjustedBOffTime and beater count is only 1
                // Readjust adjustedBOnTime to just after prev adjustedBOffTime and reduce bAnimTimeLength
                if (adjustedBOnTime < adjustedBOffTime && beaterCount == 1) {
                    float earlierDuration = adjustedBOffTime - adjustedBOnTime;

                    adjustedBOnTime = adjustedBOffTime;
                    bAnimTimeLength = bAnimTimeLength - earlierDuration;

                }
                adjustedBOffTime = adjustedBOnTime + bAnimTimeLength;

                //cout << "a time: " << aTime << " event time: " << eventTime << " c time: " << cTime;
                ///*cout << " before: " << availableBeforeHitAnimDT << " after: " << availableAfterHitAnimDT;
                //cout << " ratio: " << availableAnimTimeLengthRatio;
                //cout << " deB: " << desiredBeforeHitAnimDT << " deA: " << desiredAfterHitAnimDT;*/
                ////cout << " adjustedBOnTime: " << adjustedBOnTime << " adjustedBOffTime: " << adjustedBOffTime;
                ////cout << " anim time: " << adjustedBOffTime - adjustedBOnTime;
                //cout << " anim time: " << bAnimTimeLength;
                //cout << " key: " << keyNumber;
                //cout << endl;

                // If the previous ON note is a SIGNATURE NOTE, modify the time to its animation length
                if (iMidifile[trackIndex][eventIndex - 2].getKeyNumber() == 59) {
                    // Time to allow animation to blend out
                    float blendOutTime = 0.1;
                    float signatureAnimOnTime = adjustedBOnTime - (2 * maxSignatureAnimTimeLengths[trackIndex])
                                              - blendOutTime;
                    float signatureAnimOffTime = signatureAnimOnTime + maxSignatureAnimTimeLengths[trackIndex];
                    oMidifile[trackIndex][eventIndex - 2].tick = iMidifile.getAbsoluteTickTime(signatureAnimOnTime);
                    oMidifile[trackIndex][eventIndex - 1].tick = iMidifile.getAbsoluteTickTime(signatureAnimOffTime);
                }

                // Add bTime ON event
                vector<uchar> message;
                message.push_back(iMidifile[trackIndex][eventIndex].getCommandByte());
                message.push_back(keyNumber);
                message.push_back(velocity);
                oMidifile.addEvent(trackIndex
                    , iMidifile.getAbsoluteTickTime(adjustedBOnTime)
                    //, iMidifile.getAbsoluteTickTime(bOnTime)
                    , message);

                // Add another bTime ON event for same beat
                if (isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])) {
                    message.clear();
                    message.push_back(iMidifile[trackIndex][eventIndex + 1].getCommandByte());
                    message.push_back(iMidifile[trackIndex][eventIndex + 1].getKeyNumber());
                    message.push_back(iMidifile[trackIndex][eventIndex + 1].getVelocity());
                    oMidifile.addEvent(trackIndex
                        , iMidifile.getAbsoluteTickTime(adjustedBOnTime)
                        //, iMidifile.getAbsoluteTickTime(bOffTime)
                        , message);
                }

                // Add bTime OFF event
                message.clear();
                message.push_back(iMidifile[trackIndex][eventIndex + 1].getCommandByte());
                message.push_back(keyNumber);
                message.push_back(velocity); 
                // If the next beat is the SAME, override the message,
                // the bTime OFF event should be based on the next 2nd event
                if (isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])) {
                    message.clear();
                    message.push_back(iMidifile[trackIndex][eventIndex + 2].getCommandByte());
                    message.push_back(iMidifile[trackIndex][eventIndex + 2].getKeyNumber());
                    message.push_back(iMidifile[trackIndex][eventIndex + 2].getVelocity());
                }
                oMidifile.addEvent(trackIndex
                    , iMidifile.getAbsoluteTickTime(adjustedBOffTime)
                    //, iMidifile.getAbsoluteTickTime(bOffTime)
                    , message);

                // Add another bTime OFF event for same beat
                if (isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])) {
                    message.clear();
                    message.push_back(iMidifile[trackIndex][eventIndex + 3].getCommandByte());
                    message.push_back(iMidifile[trackIndex][eventIndex + 3].getKeyNumber());
                    message.push_back(iMidifile[trackIndex][eventIndex + 3].getVelocity());
                    oMidifile.addEvent(trackIndex
                        , iMidifile.getAbsoluteTickTime(adjustedBOffTime)
                        //, iMidifile.getAbsoluteTickTime(bOffTime)
                        , message);
                }
                
                // Cache the beat hitting time to aTime
                // So that the anim start just after the beat
                aTime = eventTime;
                // If the beaterCount is 1
                // OR the next beat is the SAME
                // Cache the end of anim time to aTime for next note calculation
                if (beaterCount == 1 
                    || isNextSameBeat(iMidifile[trackIndex][eventIndex], iMidifile[trackIndex][eventIndex + 1])
                    ) {                    
                    aTime = eventTime + allowedAfterHitAnimDT;
                }                               
            }
            
        }        
    }

    oMidifile.doTimeAnalysis();
    oMidifile.linkNotePairs();
    oMidifile.sortTracks();

    //outputLog();
    outputMIDIFile();
    
    /*cout << "Output log file." << endl;
    outputLogFile();*/

    return 0;
}

bool isPreviousSameBeat(MidiEvent beatEvent, MidiEvent prevBeatEvent) {
    return beatEvent.seconds >= (prevBeatEvent.seconds - sameBeatMarginTime)
        && beatEvent.seconds <= (prevBeatEvent.seconds + sameBeatMarginTime);
}

bool isNextSameBeat(MidiEvent beatEvent, MidiEvent nextBeatEvent) {
    return beatEvent.seconds >= (nextBeatEvent.seconds - sameBeatMarginTime)
        && beatEvent.seconds <= (nextBeatEvent.seconds + sameBeatMarginTime);
}

void outputMIDIFile() {
    string oMidifilename = iMidifilename.substr(0, iMidifilename.find('.')) + " Resync.mid";
    oMidifile.write(oMidifilename);
    cout << "Saved new file: " << oMidifilename << endl;
}

void outputLog() {
    int trackCount = oMidifile.getTrackCount();

    cout << "TPQ: " << oMidifile.getTicksPerQuarterNote() << endl;
    cout << "Total Tracks: " << trackCount << endl;
    for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {
        cout << "\nTrack: " << trackIndex << endl;
        //cout << "Tick\tTickN\tSeconds\t\tSecondsN\tDuration\tDurationN\tMessage\t\t\t\tMessageN" << endl;
        cout << "TickN\tSecondsN\tDurationN\tMessageN" << endl;
        for (int eventIndex = 0; eventIndex < oMidifile[trackIndex].getEventCount(); eventIndex++) {            
            // Get and show trackIndex name, then skip
            if (oMidifile[trackIndex][eventIndex].isTrackName()) {
                string content = oMidifile[trackIndex][eventIndex].getMetaContent();
                cout << content << endl;
                continue;
            }
            // If the eventIndex is not a note, then skip
            if (!oMidifile[trackIndex][eventIndex].isNote()) {
                continue;
            }

            /*cout << dec << iMidifile[trackIndex][eventIndex].tick;
            cout << "\t";*/
            cout << dec << oMidifile[trackIndex][eventIndex].tick;
            cout << "\t";

            /*cout << dec << iMidifile[trackIndex][eventIndex].seconds;
            cout << "\t\t";*/
            cout << dec << oMidifile.getTimeInSeconds(oMidifile[trackIndex][eventIndex].tick);
            cout << "\t\t";

            /*if (iMidifile[trackIndex][eventIndex].isNoteOn())
                cout << iMidifile[trackIndex][eventIndex].getDurationInSeconds();
            else
                cout << "--------";
            cout << "\t";*/
            if (oMidifile[trackIndex][eventIndex].isNoteOn())
                cout << oMidifile[trackIndex][eventIndex].getDurationInSeconds();
            else
                cout << "--------";
            cout << "\t";

            /*for (int i = 0; i < iMidifile[trackIndex][eventIndex].size(); i++)
                cout << (int)iMidifile[trackIndex][eventIndex][i] << ' ';
            cout << "\t\t";*/
            for (int i = 0; i < oMidifile[trackIndex][eventIndex].size(); i++)
                cout << (int)oMidifile[trackIndex][eventIndex][i] << ' ';

            cout << endl;
        }
    }
}

void outputLogFile() {
    ofstream midiLogFile;
    midiLogFile.open("MidiLog.txt", ofstream::trunc);

    int tracks = iMidifile.getTrackCount();
    midiLogFile << "TPQ: " << iMidifile.getTicksPerQuarterNote() << endl;
    if (tracks > 1) midiLogFile << "TRACKS: " << tracks << endl;
    for (int track = 0; track < tracks; track++) {
        if (tracks > 1) midiLogFile << "\nTrack " << track << endl;
        midiLogFile << "Tick\tSeconds\tDur\tMessage" << endl;
        for (int event = 0; event < iMidifile[track].size(); event++) {
            /*if (!iMidifile[track][event].isNote())
                continue;*/
            midiLogFile << dec << iMidifile[track][event].tick;
            midiLogFile << '\t' << dec << iMidifile[track][event].seconds;
            midiLogFile << '\t';
            if (iMidifile[track][event].isNoteOn())
                midiLogFile << iMidifile[track][event].getDurationInSeconds();
            midiLogFile << '\t' << hex;
            for (int i = 0; i < iMidifile[track][event].size(); i++)
                midiLogFile << (int)iMidifile[track][event][i] << ' ';
            midiLogFile << endl;
        }
    }

    midiLogFile.close();
}