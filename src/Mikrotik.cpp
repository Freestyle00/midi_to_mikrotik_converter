#include "Mikrotik.hpp"
#include "boost/log/trivial.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>

Mikrotik::Mikrotik(MidiTrack &track, 
	const uint64_t index, 
	const int octaveShift, 
	const int noteShift, 
	const double fineTuning,
	const bool commentsFlag
)
	: m_track(track), 
	m_index(index), 
	m_octaveShift(octaveShift), 
	m_noteShift(noteShift), 
	m_fineTuning(fineTuning),
	m_commentsFlag(commentsFlag)
{

}

Mikrotik::~Mikrotik()
{

}

double durationToMs(VLV vlv, const double pulsesPerSecond)
{
	return (double)(static_cast<double>(vlv.getValue()) * pulsesPerSecond);
}

void Mikrotik::setTimeCommentsAfterEachMs(const double stepMs)
{
	m_timestampMarkerStep = stepMs;
	m_nextTimestampMarker = stepMs;
}

std::string Mikrotik::getTimeAsText(const double time)
{
	std::stringstream out;
	out << std::setfill('0') << std::setw(2) << ((static_cast<int>(time)/(1000*60*60))%24) << ":";
	out << std::setfill('0') << std::setw(2) << ((static_cast<int>(time)/(1000*60))%60) << ":";
	out << std::setfill('0') << std::setw(2) << ((static_cast<int>(time)/1000)%60) << ":";
	out << std::setfill('0') << std::setw(3) << ((static_cast<int>(time)%1000));
	return out.str();
}

std::string Mikrotik::getTrackTimeLength(const uint8_t channel)
{
	std::stringstream out;
	double totalTime = m_track.getPreDelayMs();
	for(auto event : m_track.getEvents())
	{
		uint8_t cmd = event.getCmd().getMainCmd();
		if(cmd == NOTE_ON || cmd == NOTE_OFF)
		{
			if(event.getCmd().getSubCmd() == channel)
				totalTime += durationToMs(event.getDelay(), m_track.getPulsesPerSecond());
		}
		else
		{
			totalTime += durationToMs(event.getDelay(), m_track.getPulsesPerSecond());
		}
	}

	out << getTimeAsText(totalTime);

	out << " HH:MM:SS:MS";
	return out.str();
}

std::string Mikrotik::getNotesCount(const uint8_t channel)
{
	uint64_t count = 0;
	std::stringstream out;
	for(auto event : m_track.getEvents())
	{
		uint8_t cmd = event.getCmd().getMainCmd();
		if(cmd == NOTE_ON && event.getCmd().getSubCmd() == channel)
			count++;
	}
	out << count;
	return out.str();
}

std::string Mikrotik::getScriptHeader(const uint8_t channel)
{
	std::stringstream outputBuffer;
	outputBuffer << "#----------------File Description-----------------#\n";
	outputBuffer << "# This file generated by Midi To Mikrotik Converter\n";
	outputBuffer << "# Visit app repo: https://github.com/altucor/midi_to_mikrotik_converter\n";
	//outputBuffer << "# Original midi file name/path: " << m_filePath << "\n";
	outputBuffer << "# Track BPM: " << m_track.getBpm() << "\n";
	outputBuffer << "# MIDI Channel: " << std::to_string(channel) << "\n";
	outputBuffer << "# Number of notes: " << getNotesCount(channel) << "\n";
	outputBuffer << "# Track length: " << getTrackTimeLength(channel) << "\n";
	outputBuffer << "# Track name: " << m_track.getName() << "\n";
	outputBuffer << "# Instrument name: " << m_track.getInstrumentName() << "\n";
	//outputBuffer << "# Track text: " << chunk.mtrkChunkHandler.getTrackText() << "\n";
	//outputBuffer << "# Track copyright: " << chunk.mtrkChunkHandler.getCopyright() << "\n";
	//outputBuffer << "# Vocals: " << chunk.mtrkChunkHandler.getInstrumentName() << "\n";
	//outputBuffer << "# Text marker: " << chunk.mtrkChunkHandler.getTextMarker() << "\n";
	//outputBuffer << "# Cue Point: " << chunk.mtrkChunkHandler.getCuePoint() << "\n";
	outputBuffer << "#-------------------------------------------------#\n\n";
	return outputBuffer.str();
}

std::string Mikrotik::getDelayLine(const double delayMs)
{
	std::stringstream out;
	if(delayMs != 0.0)
		out << ":delay " << delayMs << "ms;\n";
	m_processedTime += delayMs;
	return out.str();
}

std::string Mikrotik::getBeepLine(Note note)
{
	std::stringstream out;
	const double freq = note.getFrequencyHz(m_octaveShift, m_noteShift, m_fineTuning);
	const double duration = durationToMs(note.getDelay(), m_track.getPulsesPerSecond());
	if(freq == 0.0)
	{
		BOOST_LOG_TRIVIAL(warning) << "Found note with zero frequency, ignoring it:";
		note.log();
	}
	if(duration == 0.0)
	{
		BOOST_LOG_TRIVIAL(warning) 
		<< "Found overlayed note ignoring it:";
		note.log();
		return out.str();
	}
	
	out << ":beep frequency=" << freq;
	out << " length=" << duration << "ms;";
	if(m_commentsFlag)
		out << " # " << note.getSymbolicNote(m_octaveShift, m_noteShift, m_fineTuning);
	out << "\n";
	return out.str();
}

std::string Mikrotik::buildNote(Note note)
{
	/*
	 * :beep frequency=440 length=1000ms;
	 * :delay 1000ms;
	 */
	
	/*
	outputBuffer << ":beep frequency=" << noteOn.getFrequencyHz();
	outputBuffer << " length=" << noteOn.getDurationPulses() << "ms;";
	if(m_commentsFlag)
		outputBuffer << " # " << noteOn.getSymbolicNote();
	outputBuffer << "\n:delay " << 
		(noteOff.getDurationPulses() + noteOn.getDurationPulses()) << "ms;\n\n";
	*/
	
	//note.log();
	std::stringstream out;
	if(note.getType() == NOTE_TYPE_ON)
		out << getBeepLine(note);
	double delay = durationToMs(note.getDelay(), m_track.getPulsesPerSecond());
	if(delay != 0.0)
	{
		out << getDelayLine(durationToMs(note.getDelay(), m_track.getPulsesPerSecond()));
		out << "\n";
	}
	return out.str();
}

std::string Mikrotik::getCurrentTimeMarker()
{
	std::stringstream out;
	out << "# Time marker: " << getTimeAsText(m_processedTime) << "\n";
	return out.str();
}

int Mikrotik::buildScriptForChannel(std::string &fileName, const uint8_t channel)
{
	std::string outFileName(fileName);
	outFileName += std::string("_");
	outFileName += m_track.getName();
	outFileName += std::string("_");
	outFileName += std::to_string(m_index);
	outFileName += std::string("_");
	outFileName += std::to_string(channel);
	outFileName += std::string(".txt");

	std::stringstream outputBuffer;
	outputBuffer << getScriptHeader(channel);
	outputBuffer << getDelayLine(m_track.getPreDelayMs());
	uint64_t foundNoteEventsCount = 0;
	for(auto event : m_track.getEvents())
	{
		uint8_t cmd = event.getCmd().getMainCmd();
		if((cmd == NOTE_ON || cmd == NOTE_OFF) && 
			event.getCmd().getSubCmd() == channel)
		{
			outputBuffer << buildNote(Note(event));
			foundNoteEventsCount++;
		}
		else
		{
			outputBuffer << getDelayLine(durationToMs(event.getDelay(), m_track.getPulsesPerSecond()));
		}
		if(m_timestampMarkerStep != 0.0)
		{
			if(m_processedTime >= m_nextTimestampMarker)
			{
				outputBuffer << getCurrentTimeMarker();
				m_nextTimestampMarker = m_processedTime + m_timestampMarkerStep;
			}
		}
	}

	if(foundNoteEventsCount == 0)
		return 0;

	BOOST_LOG_TRIVIAL(info) 
		<< "Mikrotik buildScript started for track: " 
		<< m_index << " channel: " << (uint32_t)channel;

	std::ofstream outputFile;
	outputFile.open(outFileName);
	if(!outputFile.is_open())
	{
		BOOST_LOG_TRIVIAL(info) 
			<< "Mikrotik buildScript failed cannot create output file: " 
			<< outFileName;
		return -1;
	}
	outputFile << outputBuffer.str();
	outputFile.close();
	BOOST_LOG_TRIVIAL(info) << "Mikrotik buildScript generated file: " << outFileName;
	return 0;
}

int Mikrotik::buildScript(std::string &fileName)
{
	if(m_track.getEvents().size() == 0)
		return 0;
	int ret = 0;
	for(uint8_t channel = 0; channel < 16; channel++)
	{
		ret = buildScriptForChannel(fileName, channel);
		if(ret != 0)
		{
			BOOST_LOG_TRIVIAL(info) 
			<< "Mikrotik buildScript failed on channel: " << channel;
		}
	}
	return ret;
}
