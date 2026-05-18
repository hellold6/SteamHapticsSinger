#include <iostream>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>
#include <stdint.h>

#include <signal.h>
#include <stdio.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <windows.h>
#endif

#include <hidapi.h>
#include <libusb.h>
#include "midifile/midifile.h"

#define STEAM_CONTROLLER_MAGIC_PERIOD_RATIO 495483.0
#define CHANNEL_COUNT					   4
#define DEFAULT_INTERVAL_USEC			   10000

#define DURATION_MAX		-1
#define NOTE_STOP		   -1

#define DEFAULT_GAIN 127

using namespace std;

double midiFrequency[128]  = {0, 8.66196, 9.17702, 9.72272, 10.3009, 10.9134, 11.5623, 12.2499, 12.9783, 13.75, 14.5676, 15.4339, 16.3516, 17.3239, 18.354, 19.4454, 20.6017, 21.8268, 23.1247, 24.4997, 25.9565, 27.5, 29.1352, 30.8677, 32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 2093, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520, 3729.31, 3951.07, 4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040, 7458.62, 7902.13, 8372.02, 8869.84, 9397.27, 9956.06, 10548.1, 11175.3, 11839.8, 12543.9};


struct ParamsStruct{
	const char* midiSong;
	const char* captureMidiOutput;
	std::string midiSongStorage;
	unsigned int intervalUSec;
	unsigned int captureDurationSec;
	int libusbDebugLevel;
	bool repeatSong;
	bool captureSystemAudio;
	float volume;
#ifdef _WIN32
	std::string winAudioDevice;
#endif
};

//TEMPORARY, move to ParamsStruct and find a way to reference within playback function
bool legacyInst = false;
bool directVel = false;
bool tritonLimit = false;
bool tritonSwap = false;
int channelCount = 2;

enum class ControllerType {
	None,
	Original,
	Triton,
	Jupiter,
	Galileo
};

struct SteamControllerInfos{
	libusb_device_handle* dev_handle;
	hid_device* hid_handle;
	int interfaceNum;
	ControllerType type = ControllerType::None;
};

SteamControllerInfos steamController1;

bool SteamController_Open(SteamControllerInfos* controller){
	if(!controller) return false;

	struct hid_device_info *devs, *cur_dev;
	unsigned char buf[64];
	//Open Steam Controller device
	if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1101)) != NULL){ // A Steam Controller
		cout<<"Found a Steam Controller"<<endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1102)) != NULL){ // Wired Steam Controller (2015)
		cout<<"Found wired Steam Controller (2015)"<<endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1142)) != NULL){ // Steam Controller (2015) dongle //TODO: FIX
		cout<<"Found Steam Dongle, will attempt to use the first Steam Controller (2015)"<<endl;
		controller->interfaceNum = 1;
		controller->type = ControllerType::Original;
	} 
	else if((controller->hid_handle = hid_open(0x28DE, 0x1302, NULL)) != NULL) { // Steam Controller (2026)
		cout<<"Found wired Steam Controller (2026)"<<endl;
		controller->type = ControllerType::Triton;
		if (!tritonLimit) channelCount = 4;
	}
	else if((devs = hid_enumerate(0x28DE, 0x1304)) != NULL) { // Steam Puck
		cout<<"Found Steam Puck, attempting to find first Steam Controller (2026)... ";
		
		cur_dev = devs;
		while (cur_dev) {
			if (cur_dev->vendor_id == 0x28DE && cur_dev->product_id == 0x1304) {
				controller->hid_handle = hid_open_path(cur_dev->path);
				if(controller->hid_handle) {
					int res = hid_read_timeout(controller->hid_handle,buf,sizeof(buf),100);
					if (res > 0) {
						cout << "OK" << endl;
						break;
					}
				}
			}
			cur_dev = cur_dev->next;
		}
		
		hid_free_enumeration(devs);
		
		if(!cur_dev) {
			cout<<endl<<"No controller connected / found"<<endl;
			return false;
		}
		
		controller->type = ControllerType::Triton;
		if (!tritonLimit) channelCount = 4;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1205)) != NULL){ // Steam Deck
		cout<<"Found Steam Deck"<<endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Jupiter;
	}
	else{
		cout<<"No device found"<<endl;
		return false;
	}

	//If dev_handle is NULL, it's using HIDAPI so skip this
	if(controller->dev_handle != NULL) {
		//On Linux, automatically detach and reattach kernel module
		libusb_set_auto_detach_kernel_driver(controller->dev_handle,1);
		//Claim the USB interface
		int r = libusb_claim_interface(controller->dev_handle,controller->interfaceNum);
		if(r < 0) {
			cout<<"Interface claim Error "<<libusb_error_name(r)<<endl;
			std::cin.ignore();
			libusb_close(controller->dev_handle);
			return false;
		}
	}
	
	return true;
}

void SteamController_Close(SteamControllerInfos* controller){
	if(controller->dev_handle != NULL) {
		int r = libusb_release_interface(controller->dev_handle,controller->interfaceNum);
		if(r < 0) {
			cout<<"Interface release Error "<<libusb_error_name(r)<<endl;
			std::cin.ignore();
			return;
		}
		libusb_close(controller->dev_handle);
	} else {
		hid_close(controller->hid_handle);
	}
}

//Steam Haptics Playblack
int SteamHaptics_PlayNote(SteamControllerInfos* controller, int channel, int note, int velocity){
	if (channel > 1 && controller->type != ControllerType::Triton) return 1;
	unsigned char dataBlob[65] = {0};
	
	double frequency = midiFrequency[note];
	uint16_t duration = (note == NOTE_STOP) ? 0x0000 : 0x7fff;

	int r;

	double period;
	uint16_t periodCommand;
	uint16_t repeatCommand;
	uint16_t gainCommand;

	switch(controller->type) {
	case ControllerType::Original: //Steam Controller (2015) Playback

		period = 1.0 / frequency;
		periodCommand = period * STEAM_CONTROLLER_MAGIC_PERIOD_RATIO; //Reminder to check if the Steam Controller tuning lines up with the Deck.
		repeatCommand = (note == NOTE_STOP) ? 0x0000 : 0x7fff;
		//gainCommand = (directVel) ? (velocity * 65535) / 127 : 0x0000; //Doesn't work

		dataBlob[0] = 0x8F;
		dataBlob[2] = channel;
		dataBlob[3] = periodCommand % 0xFF;
		dataBlob[4] = periodCommand / 0xFF;
		dataBlob[5] = periodCommand % 0xFF;
		dataBlob[6] = periodCommand / 0xFF;
		dataBlob[7] = repeatCommand % 0xFF;
		dataBlob[8] = repeatCommand / 0xFF;
		//dataBlob[9] = 0x00;
		//dataBlob[10]= 0x00;
		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,controller->interfaceNum,dataBlob,64,1000);
		if(r < 0) {
			cout<<"Command Error "<<libusb_error_name(r)<< endl;
			exit(0);
		}
		break;

	case ControllerType::Triton: //Steam Controller (2026) Playback

		if (note == NOTE_STOP) {
			//This prevents the controller from rebooting when using rumble motors and drifting out of tune
			dataBlob[0] = 0x81;
			dataBlob[1] = (tritonSwap) ?
						  ((channel < 2) ? channel : !(channel-2)+3) :
						  ((channel < 2) ? !channel+3 : channel-2);			  
			//dataBlob[1] = ((channel < 2) != tritonSwap) ? !channel+3 : channel-2;
		} else {
			dataBlob[0] = 0x83;
			dataBlob[1] = (tritonSwap) ?
						  ((channel < 2) ? !channel : !(channel-2)+3) :
						  ((channel < 2) ? !channel+3 : !(channel-2));
			//dataBlob[1] = ((channel < 2) != tritonSwap) ? !channel+3 : !(channel-2);
			dataBlob[2] = (directVel) ? (velocity * 255) / 127 - 128 : 0xFE;
			dataBlob[3] = (int)frequency % 0xFF;
			dataBlob[4] = (int)frequency / 0xFF;
			dataBlob[5] = 0xFF;
			dataBlob[6] = 0x7F;
		}
		
		r = hid_write(controller->hid_handle,dataBlob,65);
		if(r < 0) {
			const wchar_t* error_msg = hid_error(controller->hid_handle);
			wcout<<"Command Error "<<error_msg<< endl;
			exit(0);
		}
		break;

	case ControllerType::Jupiter: //Steam Deck Playback
	
		dataBlob[0] = 0xEA;
		dataBlob[2] = !channel; //Swap haptics to match 2015
		dataBlob[3] = 0x03; 
		dataBlob[5] = (directVel) ? (velocity * 255) / 127 - 128 : 0x00;
		dataBlob[6] = (int)frequency % 0xFF;
		dataBlob[7] = (int)frequency / 0xFF;
		dataBlob[8] = duration % 0xFF;
		dataBlob[9] = duration / 0xFF;
		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,2,dataBlob,64,1000);
		if(r < 0) {
			cout<<"Command Error "<<libusb_error_name(r)<< endl;
			exit(0);
		}
		break;
	
	}

	return 0;
}

float timeElapsedSince(std::chrono::steady_clock::time_point tOrigin){
	using namespace std::chrono;
	steady_clock::time_point tNow = steady_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(tNow - tOrigin);
	return time_span.count();
}

void setMidiSong(ParamsStruct* params, const std::string& midiPath){
	params->midiSongStorage = midiPath;
	params->midiSong = params->midiSongStorage.c_str();
}

bool copyFile(const std::string& source, const std::string& destination){
	FILE* sourceFile = fopen(source.c_str(), "rb");
	if(!sourceFile) return false;
	FILE* destinationFile = fopen(destination.c_str(), "wb");
	if(!destinationFile){
		fclose(sourceFile);
		return false;
	}

	unsigned char buffer[8192];
	size_t bytesRead;
	while((bytesRead = fread(buffer, 1, sizeof(buffer), sourceFile)) > 0){
		if(fwrite(buffer, 1, bytesRead, destinationFile) != bytesRead){
			fclose(sourceFile);
			fclose(destinationFile);
			return false;
		}
	}

	fclose(sourceFile);
	fclose(destinationFile);
	return true;
}

#ifndef _WIN32
bool runCommand(const std::vector<std::string>& commandArgs){
	if(commandArgs.empty()) return false;

	std::vector<char*> argv;
	for(const std::string& arg : commandArgs){
		argv.push_back(const_cast<char*>(arg.c_str()));
	}
	argv.push_back(nullptr);

	pid_t pid = fork();
	if(pid < 0) return false;
	if(pid == 0){
		execvp(argv[0], argv.data());
		_exit(127);
	}

	int status = 0;
	if(waitpid(pid, &status, 0) < 0) return false;
	return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

bool removeDirectoryRecursively(const std::string& directoryPath){
	DIR* directory = opendir(directoryPath.c_str());
	if(!directory) return false;

	bool isSuccess = true;
	struct dirent* entry = nullptr;
	while((entry = readdir(directory)) != nullptr){
		if((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)) continue;

		std::string childPath = directoryPath + "/" + entry->d_name;
		struct stat childStat = {0};
		if(lstat(childPath.c_str(), &childStat) != 0){
			isSuccess = false;
			continue;
		}

		if(S_ISDIR(childStat.st_mode)){
			if(!removeDirectoryRecursively(childPath)) isSuccess = false;
		}
		else{
			if(unlink(childPath.c_str()) != 0) isSuccess = false;
		}
	}

	closedir(directory);
	if(rmdir(directoryPath.c_str()) != 0) isSuccess = false;
	return isSuccess;
}

bool captureSystemAudioToMidi(const ParamsStruct& params, std::string& generatedMidiPath){
	std::string outputMidiPath = params.captureMidiOutput;

	char tempAudioTemplate[] = "/tmp/steam-haptics-singer-audio-XXXXXX.wav";
	int tempAudioFd = mkstemps(tempAudioTemplate, 4);
	if(tempAudioFd < 0){
		cout << "Unable to create temporary audio file." << endl;
		return false;
	}
	close(tempAudioFd);
	std::string tempAudioPath = tempAudioTemplate;

	char tempMidiDirTemplate[] = "/tmp/steam-haptics-singer-midi-XXXXXX";
	char* tempMidiDir = mkdtemp(tempMidiDirTemplate);
	if(tempMidiDir == nullptr){
		cout << "Unable to create temporary MIDI directory." << endl;
		unlink(tempAudioPath.c_str());
		return false;
	}
	std::string tempMidiDirectory = tempMidiDir;

	if(!runCommand({"ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "pulse", "-i", "default", "-t", std::to_string(params.captureDurationSec), tempAudioPath})){
		cout << "Audio capture failed. Ensure ffmpeg is installed and system audio capture is accessible." << endl;
		unlink(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	if(!runCommand({"basic-pitch", tempMidiDirectory, tempAudioPath})){
		cout << "Audio-to-MIDI transcription failed. Ensure basic-pitch is installed and available in PATH." << endl;
		unlink(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	size_t slashIndex = tempAudioPath.find_last_of('/');
	std::string tempAudioName = (slashIndex == std::string::npos) ? tempAudioPath : tempAudioPath.substr(slashIndex + 1);
	size_t extensionIndex = tempAudioName.find_last_of('.');
	std::string tempAudioBaseName = (extensionIndex == std::string::npos) ? tempAudioName : tempAudioName.substr(0, extensionIndex);
	std::string generatedTempMidiPath = tempMidiDirectory + "/" + tempAudioBaseName + "_basic_pitch.mid";

	if(!copyFile(generatedTempMidiPath, outputMidiPath)){
		cout << "Failed to copy generated MIDI file to output path: " << outputMidiPath << endl;
		unlink(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	unlink(tempAudioPath.c_str());
	removeDirectoryRecursively(tempMidiDirectory);

	generatedMidiPath = outputMidiPath;
	return true;
}
#endif // !_WIN32

#ifdef _WIN32
// Escape a single argument for the Windows command-line (CommandLineToArgvW-compatible).
static std::string windowsQuoteArg(const std::string& arg){
	// No quoting needed if the argument has no problematic characters
	if(!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos){
		return arg;
	}
	std::string result = "\"";
	for(auto it = arg.begin(); ; ++it){
		int backslashCount = 0;
		while(it != arg.end() && *it == '\\'){
			++it;
			++backslashCount;
		}
		if(it == arg.end()){
			// Backslashes at the end: double them before the closing quote
			result.append(backslashCount * 2, '\\');
			break;
		}
		else if(*it == '"'){
			// Backslashes before a quote: double them, then escape the quote
			result.append(backslashCount * 2 + 1, '\\');
			result.push_back('"');
		}
		else{
			// Ordinary character: backslashes are not special
			result.append(backslashCount, '\\');
			result.push_back(*it);
		}
	}
	result.push_back('"');
	return result;
}

bool runCommand(const std::vector<std::string>& commandArgs){
	if(commandArgs.empty()) return false;

	std::string commandLine;
	for(size_t i = 0; i < commandArgs.size(); i++){
		if(i > 0) commandLine += " ";
		commandLine += windowsQuoteArg(commandArgs[i]);
	}

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	std::vector<char> cmdBuf(commandLine.begin(), commandLine.end());
	cmdBuf.push_back('\0');

	if(!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)){
		cout << "Failed to start process (error " << GetLastError() << "): " << commandArgs[0] << endl;
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 1;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return exitCode == 0;
}

bool removeDirectoryRecursively(const std::string& directoryPath);

bool fileExists(const std::string& path){
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}


bool removeDirectoryRecursively(const std::string& directoryPath){
	WIN32_FIND_DATAA findData;
	std::string searchPath = directoryPath + "\\*";
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
	if(hFind == INVALID_HANDLE_VALUE) return false;

	bool isSuccess = true;
	do{
		if(strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
		std::string childPath = directoryPath + "\\" + findData.cFileName;
		if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			if(!removeDirectoryRecursively(childPath)) isSuccess = false;
		}
		else{
			if(!DeleteFileA(childPath.c_str())) isSuccess = false;
		}
	} while(FindNextFileA(hFind, &findData));

	FindClose(hFind);
	if(!RemoveDirectoryA(directoryPath.c_str())) isSuccess = false;
	return isSuccess;
}

bool captureSystemAudioToMidi(const ParamsStruct& params, std::string& generatedMidiPath){
	std::string outputMidiPath = params.captureMidiOutput;

	char tempPath[MAX_PATH];
	if(GetTempPathA(MAX_PATH, tempPath) == 0){
		cout << "Unable to get temporary directory." << endl;
		return false;
	}

	// Create a uniquely named temp audio file with .wav extension.
	// GetTempFileNameA atomically creates and holds a .TMP file; rename it to .wav to avoid
	// a delete-then-create race while still keeping a guaranteed-unique name.
	char tempAudioBuf[MAX_PATH];
	if(GetTempFileNameA(tempPath, "aud", 0, tempAudioBuf) == 0){
		cout << "Unable to create temporary audio file." << endl;
		return false;
	}
	std::string tempAudioPath = std::string(tempAudioBuf);
	size_t dotPos = tempAudioPath.rfind('.');
	std::string tempAudioWavPath = (dotPos != std::string::npos) ? tempAudioPath.substr(0, dotPos) + ".wav" : tempAudioPath + ".wav";
	if(MoveFileA(tempAudioBuf, tempAudioWavPath.c_str())){
		tempAudioPath = tempAudioWavPath;
	}
	// If rename fails the .TMP path is still used; ffmpeg writes WAV regardless of extension.

	// Create a uniquely named temp MIDI directory
	char tempMidiDirBuf[MAX_PATH];
	if(GetTempFileNameA(tempPath, "mid", 0, tempMidiDirBuf) == 0){
		cout << "Unable to create temporary MIDI directory." << endl;
		DeleteFileA(tempAudioPath.c_str());
		return false;
	}
	DeleteFileA(tempMidiDirBuf);
	std::string tempMidiDirectory = tempMidiDirBuf;
	if(!CreateDirectoryA(tempMidiDirectory.c_str(), NULL)){
		cout << "Unable to create temporary MIDI directory." << endl;
		DeleteFileA(tempAudioPath.c_str());
		return false;
	}

	std::string dshowDevice = "audio=" + (params.winAudioDevice.empty() ? "Stereo Mix" : params.winAudioDevice);
	bool captureSucceeded = runCommand({"ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
	                                    "-f", "dshow",
	                                    "-i", dshowDevice,
	                                    "-t", std::to_string(params.captureDurationSec), tempAudioPath});

	if(!captureSucceeded){
		cout << "Audio capture failed. Ensure ffmpeg is installed and a DirectShow audio device is available.\n"
		     << "  Use -w \"Device Name\" to specify a capture device (device names with spaces are handled automatically).\n"
		     << "  Run 'ffmpeg -f dshow -list_devices true -i dummy' to list available devices.\n"
		     << "  You may need to enable 'Stereo Mix' in Windows Sound settings (right-click the speaker icon -> Sounds -> Recording tab)." << endl;
		DeleteFileA(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	if(!runCommand({"basic-pitch", tempMidiDirectory, tempAudioPath})){
		cout << "Audio-to-MIDI transcription failed. Ensure basic-pitch is installed and available in PATH." << endl;
		DeleteFileA(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	size_t slashIndex = tempAudioPath.find_last_of("/\\");
	std::string tempAudioName = (slashIndex == std::string::npos) ? tempAudioPath : tempAudioPath.substr(slashIndex + 1);
	size_t extensionIndex = tempAudioName.find_last_of('.');
	std::string tempAudioBaseName = (extensionIndex == std::string::npos) ? tempAudioName : tempAudioName.substr(0, extensionIndex);
	std::string generatedTempMidiPath = tempMidiDirectory + "\\" + tempAudioBaseName + "_basic_pitch.mid";

	if(!copyFile(generatedTempMidiPath, outputMidiPath)){
		cout << "Failed to copy generated MIDI file to output path: " << outputMidiPath << endl;
		DeleteFileA(tempAudioPath.c_str());
		removeDirectoryRecursively(tempMidiDirectory);
		return false;
	}

	DeleteFileA(tempAudioPath.c_str());
	removeDirectoryRecursively(tempMidiDirectory);

	generatedMidiPath = outputMidiPath;
	return true;
}
#endif // _WIN32


void displayPlayedNotes(int channel, int8_t note){
	static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
	const char* textPerChannel[CHANNEL_COUNT] = {"LEFT haptic : ",", RIGHT haptic : ",", LEFT haptic : ",", RIGHT haptic : "};
	const char* noteBaseNameArray[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

	if(channel >= channelCount)
		return;

	notePerChannel[(channel < 2) ? !channel : !(channel-2)+2] = note;

	for(int i = 0 ; i < channelCount ; i++){
		cout << textPerChannel[i];

		//Write empty string
		if(notePerChannel[i] == NOTE_STOP){
			cout << "OFF ";
		}
		else{
			//Write note name
			cout << noteBaseNameArray[notePerChannel[i]%12];
			int octave = (notePerChannel[i]/12)-1;
			cout << octave;
			if(octave >= 0 ){
				cout << " ";
			}
		}
	}

	cout << "\r" ;
	cout.flush();
}

void playSong(SteamControllerInfos* controller,const ParamsStruct params){

	MidiFile_t midifile;

	//Open Midi File
	midifile = MidiFile_load(const_cast<char*>(params.midiSong));

	if(midifile == NULL){
		cout << "Unable to open MIDI file!" << params.midiSong << endl;
		return;
	}

	//Check if file contains at least one midi event
	if(MidiFile_getFirstEvent(midifile) == NULL){
		cout << "MIDI file is empty!" << endl;
		return;
	}
	
	if (strstr(params.midiSong,"dv")) {
        std::cout << "Found \"dv\" in file name, assuming direct velocity to gain control" << std::endl;
		directVel = true;
    }

	//Waiting for user to press enter; YOURE WRONG, SULFURIC ACID!
	cout << "Starting playback of " << params.midiSong  << "... press Ctrl+C anytime to stop" << endl;
	sleep(1);

	//This will contains the previous events accepted for each channel
	MidiFileEvent_t acceptedEventPerChannel[CHANNEL_COUNT] = {0};

	//Get current time point, will be used to know elapsed time
	std::chrono::steady_clock::time_point tOrigin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point tRestart = std::chrono::steady_clock::now();

	//Iterate through events
	MidiFileEvent_t currentEvent = MidiFile_getFirstEvent(midifile);
	
	while(currentEvent != NULL){
		usleep(params.intervalUSec);

		//This will contains the events to play
		MidiFileEvent_t eventsToPlay[CHANNEL_COUNT] = {NULL};

		//We now need to play all events with tick < currentTime
		long currentTick = MidiFile_getTickFromTime(midifile,timeElapsedSince(tOrigin));

		//Iterate through all events until the current time, and selecte potential events to play
		for( ; currentEvent != NULL && MidiFileEvent_getTick(currentEvent) < currentTick ; currentEvent = MidiFileEvent_getNextEventInFile(currentEvent)){

			//Only process note start events or note end events matching previous event
			if (!MidiFileEvent_isNoteStartEvent(currentEvent) && !MidiFileEvent_isNoteEndEvent(currentEvent)) continue;

			//Get channel event
			int eventChannel = MidiFileVoiceEvent_getChannel(currentEvent);

			//If channel is other than 0 or 1, skip this event, we cannot play it with only 1 steam controller
			if(eventChannel < 0 || !(eventChannel < channelCount)) continue;

			//If event is note off and does not match previous played event, skip it
			if(MidiFileEvent_isNoteEndEvent(currentEvent)){
				MidiFileEvent_t previousEvent = acceptedEventPerChannel[eventChannel];

				//Skip if current event is not ending previous event,
				// or if they share the same tick ( end event after start evetn on same tick )
				if(MidiFileNoteStartEvent_getNote(previousEvent) != MidiFileNoteEndEvent_getNote(currentEvent)
				||(MidiFileEvent_getTick(currentEvent) == MidiFileEvent_getTick(previousEvent)))
					continue;
			}

			//If we arrive here, this event is accepted
			eventsToPlay[eventChannel] = currentEvent;
			acceptedEventPerChannel[eventChannel]=currentEvent;
		}

		//Now play the last events found
		for(int currentChannel = 0 ; currentChannel < channelCount ; currentChannel++){
			MidiFileEvent_t selectedEvent = eventsToPlay[currentChannel];

			//If no note event available on the channel, skip it
			if(!MidiFileEvent_isNoteStartEvent(selectedEvent) && !MidiFileEvent_isNoteEndEvent(selectedEvent)) continue;

			//Set note event
			int8_t eventNote = NOTE_STOP;
			int8_t eventVel  = 0;
			if(MidiFileEvent_isNoteStartEvent(selectedEvent)){
				//Send note stop before playing to prevent Steam Controller (2026) rebooting when using motors
				SteamHaptics_PlayNote(controller,currentChannel,NOTE_STOP,0);
				eventNote = MidiFileNoteStartEvent_getNote(selectedEvent);
				eventVel  = MidiFileNoteStartEvent_getVelocity(selectedEvent);
				eventVel  = (int8_t)std::min((int)(eventVel * params.volume), 127);
			}

			//Play notes
			SteamHaptics_PlayNote(controller,currentChannel,eventNote,eventVel);

			displayPlayedNotes(currentChannel,eventNote);
		}
	}

	for(int i = 0 ; i < CHANNEL_COUNT ; i++){
		SteamHaptics_PlayNote(&steamController1,i,NOTE_STOP,0); //Wait, this actually references the controller directly, why????????
	}
	
	cout <<endl<< "Playback completed " << endl;
}





bool parseArguments(int argc, char** argv, ParamsStruct* params){
	int c;
#ifndef _WIN32
	while ( (c = getopt(argc, argv, "d:i:a:o:petsv:")) != -1) {
#else
	while ( (c = getopt(argc, argv, "d:i:a:o:petsw:v:")) != -1) {
#endif
		unsigned long int value;
		switch(c){
		/*case 'l':
			value = strtoul(optarg,NULL,10);
			if(value <= 255 && value > 0){
				params->leftGain = value;
			}
			break;
		case 'r':
			value = strtoul(optarg,NULL,10);
			if(value <= 255 && value > 0){
				params->rightGain = value;
			}
			break;*/
		case 'd':
			value = strtoul(optarg,NULL,10);
			if(value >= LIBUSB_LOG_LEVEL_NONE && value <= LIBUSB_LOG_LEVEL_DEBUG){
				params->libusbDebugLevel = value;
			}
			break;
		case 'i':
			value = strtoul(optarg,NULL,10);
			if(value <= 1000000 && value > 0){
				params->intervalUSec = value;
			}
			break;
		case 'a':
			value = strtoul(optarg,NULL,10);
			if(value > 0){
				params->captureDurationSec = value;
				params->captureSystemAudio = true;
			}
			break;
		case 'o':
			params->captureMidiOutput = optarg;
			break;
		case 'p':
			params->repeatSong = true;
			break;
		case 'e':
			directVel = true;
			break;
		case 't':
			tritonLimit = true;
			break;
		case 's':
			tritonSwap = true;
			break;
		case 'v':
			{
				unsigned long int pct = strtoul(optarg, NULL, 10);
				if(pct > 0 && pct <= 500){
					params->volume = pct / 100.0f;
				}
			}
			break;
#ifdef _WIN32
		case 'w':
			params->winAudioDevice = optarg;
			break;
#endif
		// case 'y':
		// 	legacyInst = true;
		// 	break;
		case '?':
			return false;
			break;
		default:
			break;
		}
	}
	return params->captureSystemAudio;
}


void abortPlaying(int){
	for(int i = 0 ; i < CHANNEL_COUNT ; i++){
		SteamHaptics_PlayNote(&steamController1,i,NOTE_STOP,0); //Wait, this actually references the controller directly, why????????
	}

	SteamController_Close(&steamController1);

	cout << endl<< "Aborted " << endl;
	cout.flush();
	exit(1);
}

int main(int argc, char** argv)
{
	cout <<"Steam Haptics Singer by Crazy, Steam Controller Singer by Pila"<<endl;

	ParamsStruct params;
	params.intervalUSec = DEFAULT_INTERVAL_USEC;
	params.libusbDebugLevel = LIBUSB_LOG_LEVEL_NONE;
	params.repeatSong = false;
	params.midiSong = "\0";
	params.captureMidiOutput = "captured-audio.mid";
	params.captureDurationSec = 15;
	params.captureSystemAudio = false;
	params.volume = 1.0f;
#ifdef _WIN32
	// Empty string = use "Stereo Mix" DirectShow device; override with -w for a specific device.
	params.winAudioDevice = "";
#endif
	//params.leftGain = DEFAULT_GAIN;
	//params.rightGain = DEFAULT_GAIN;


	//Parse arguments
	if(!parseArguments(argc, argv, &params)){
		cout << "Usage: steam-haptics-singer -a SECONDS [-o OUTPUT_MIDI] [options]\n"
			  "\n  -a SECONDS      Capture system audio for N seconds and transcribe it to MIDI before playback (Linux: PulseAudio; Windows: DirectShow)"
			  "\n  -o OUTPUT_MIDI  Output path for generated MIDI. Default: captured-audio.mid"
			  "\n  -i INTERVAL     Player sleep interval (in microseconds). Default: 10000"
			  "\n  -d DEBUG_LEVEL  Libusb debug level (0-4). Default: 0"
			  "\n  -p              Repeat playback after ending"
			  "\n  -v PERCENT      Volume multiplier as percentage (1-500). Default: 100"
			  "\n  -e              Direct velocity to gain control"
			  "\n  -t              (Steam Controller 2026 Only) Limit to only two channels"
			  "\n  -s              (Steam Controller 2026 Only) Swap rumble and trackpad channels"
#ifdef _WIN32
			  "\n  -w DEVICE_NAME  (Windows) DirectShow audio device name for capture. Default: Stereo Mix"
			  "\n                   Run 'ffmpeg -f dshow -list_devices true -i dummy' to list available devices."
			  "\n                   You may need to enable 'Stereo Mix' in Windows Sound settings."
#endif
			  "" << endl;
		return 1;
	}

	std::string generatedMidiPath;
	if(params.captureSystemAudio){
		cout << "Capturing system audio for " << params.captureDurationSec << " seconds..." << endl;
		if(!captureSystemAudioToMidi(params, generatedMidiPath)){
			return 1;
		}
		setMidiSong(&params, generatedMidiPath);
		cout << "Generated MIDI file: " << params.midiSong << endl;
	}


	//Initializing LIBUSB
	int r = libusb_init(NULL);
	if(r < 0) {
		cerr<<"LIBUSB Init Error "<<libusb_error_name(r)<<endl;
		cin.ignore();
		return 1;
	}

	//Initializing HIDAPI
    if (hid_init() != 0) {
        cerr<<"HIDAPI Init Error "<<endl;
		cin.ignore();
        return 1;
    }

	libusb_set_debug(NULL, params.libusbDebugLevel);

	//Gaining access to Steam Controller
	if(!SteamController_Open(&steamController1)){
		return 1;
	}

	//Set mecanism to stop playing when closing process
	signal(SIGINT, abortPlaying);

	//Playing song
	do{
		playSong(&steamController1,params);
	}while(params.repeatSong);


	//Releasing access to Steam Controller
	SteamController_Close(&steamController1);

	libusb_exit(NULL);
	hid_exit();

	return 0;
}
