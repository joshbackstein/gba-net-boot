#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <3ds.h>

static const char* APP_TITLE = "GBA Net Boot v0.0.2 Beta";
static bool ENABLE_DEBUG = false;

#define FCRAM_START_OLD_FW (0x14000000)
#define FCRAM_START_NEW_FW (0x30000000)
#define FIRM_MAX_SIZE	(0x400000 - 0x200)
#define FIRM_OFFSET		(0x1000)
void* firm_buffer = NULL;

static const char* DEFAULT_FIRM_PATH = "open_agb_firm.firm";
static const char* LUMA_FIRM_PATH = "/luma/payloads/open_agb_firm.firm";

static FS_Archive sdmcArchive;
static FS_Path ROM_PATH;
static FS_Path FINAL_ROM_PATH;
static Handle rom_fd;

static const char* ROM_PATH_STR = "/rom.gba.tmp";
static const char* FINAL_ROM_PATH_STR = "/rom.gba";
static const s32 MAX_ROM_SIZE = 1024*1024*32;

#define PORT 31313
static const char* INIT_REQUEST = "gba_net_boot_init_beta_0001";
static const char* INIT_RESPONSE = "gba_net_boot_ack_beta_0001";

// 1 MiB for SOC buffer
#define SOC_BUFFER_SIZE	0x100000
#define SOC_ALIGN				0x1000
static u32* SOC_buffer = NULL;

// 8 MiB for TCP buffer
#define TCP_BUFFER_SIZE	0x800000
static u8* TCP_buffer = NULL;
static s32 TCP_buffer_offset = 0;
static s32 file_size = 0;
static u32 bytes_written = 0;

// 256 bytes for UDP buffer
#define UDP_BUFFER_SIZE 0x100
static u8* UDP_buffer = NULL;

static s32 tcp_listener = -1;
static s32 tcp_downloader = -1;
static s32 udp_listener = -1;

void failExit(const char* fmt, ...);

void initSdmcStructs() {
	Result ret;
	ret = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
	if (R_FAILED(ret)) {
		failExit("could not open sdmc archive\n");
	}
	ROM_PATH = fsMakePath(PATH_ASCII, ROM_PATH_STR);
	FINAL_ROM_PATH = fsMakePath(PATH_ASCII, FINAL_ROM_PATH_STR);
}

void deinitSdmcStructs() {
	Result ret;
	ret = FSUSER_CloseArchive(sdmcArchive);
	if (R_FAILED(ret)) {
		failExit("could not close sdmc archive\n");
	}
}

void checkFirmAddress() {
	printf("Firm at 0x%08lx\n", (u32) firm_buffer + FIRM_OFFSET);
	if ((u32) firm_buffer != FCRAM_START_OLD_FW && (u32) firm_buffer != FCRAM_START_NEW_FW) {
		failExit("Bad firm location\n");
	}
}

void loadOafFirm() {
	printf("\nLoading %s\n", DEFAULT_FIRM_PATH);
	FILE* firm_fd = fopen(DEFAULT_FIRM_PATH, "rb");
	if (firm_fd) {
		printf(CONSOLE_GREEN);
		printf("Success\n");
		printf(CONSOLE_RESET);
	} else {
		printf("Could not open file\n");
		printf("\nLoading %s\n", LUMA_FIRM_PATH);
		firm_fd = fopen(LUMA_FIRM_PATH, "rb");
	}
	if (firm_fd) {
		printf(CONSOLE_GREEN);
		printf("Success\n");
		printf(CONSOLE_RESET);
	} else {
		printf("Could not open file\n");
	}
	if (firm_fd == NULL) {
		failExit("Could not load any firm file\n");
	}

	// Get size of file
	fseek(firm_fd, 0, SEEK_END);
	s32 firm_size = ftell(firm_fd);
	if (firm_size == -1) {
		fclose(firm_fd);
		failExit("Could not determine firm size\n");
	}
	printf("Firm size: %ld\n", firm_size);
	printf("Max firm size: %d\n", FIRM_MAX_SIZE);
	if (firm_size > FIRM_MAX_SIZE) {
		fclose(firm_fd);
		failExit("Selected firm too large\n");
	}

	// Go to beginning of file
	fseek(firm_fd, 0, SEEK_SET);

	// Copy into buffer
	printf("\nCopying firm into memory\n");
	s32 bytes_read = fread(firm_buffer + FIRM_OFFSET, 1, firm_size, firm_fd);
	if (bytes_read != firm_size) {
		fclose(firm_fd);
		failExit("Could not copy firm into memory\n");
	}

	printf(CONSOLE_GREEN);
	printf("Success\n");
	printf(CONSOLE_RESET);
	fclose(firm_fd);
}

void openRom() {
	Result ret;
	ret = FSUSER_OpenFile(&rom_fd, sdmcArchive, ROM_PATH, FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
	if (R_FAILED(ret)) {
		failExit("could not open ROM for writing\n");
	}
}

void closeRom() {
	Result ret;
	ret = FSFILE_Close(rom_fd);
	if (R_FAILED(ret) && R_SUMMARY(ret) != RS_INVALIDARG) {
		if (ENABLE_DEBUG) {
			printf("Result %ld\n", ret);
			printf("R_LEVEL %ld\n", R_LEVEL(ret));
			printf("R_SUMMARY %ld\n", R_SUMMARY(ret));
			printf("R_MODULE %ld\n", R_MODULE(ret));
			printf("R_DESCRIPTION %ld\n", R_DESCRIPTION(ret));
			printf("errno: %d\n", errno);
			printf("errno str: %s\n", strerror(errno));
		}
		failExit("could not close ROM\n");
	}
}

void moveRomToFinalPath() {
	Result ret;
	ret = FSUSER_DeleteFile(sdmcArchive, FINAL_ROM_PATH);
	if (R_FAILED(ret) && R_SUMMARY(ret) != RS_NOTFOUND) {
		if (ENABLE_DEBUG) {
			printf("Result %ld\n", ret);
			printf("R_LEVEL %ld\n", R_LEVEL(ret));
			printf("R_SUMMARY %ld\n", R_SUMMARY(ret));
			printf("R_MODULE %ld\n", R_MODULE(ret));
			printf("R_DESCRIPTION %ld\n", R_DESCRIPTION(ret));
			printf("errno: %d\n", errno);
			printf("errno str: %s\n", strerror(errno));
		}
		failExit("could not delete existing ROM\n");
	}
	ret = FSUSER_RenameFile(sdmcArchive, ROM_PATH, sdmcArchive, FINAL_ROM_PATH);
	if (R_FAILED(ret)) {
		if (ENABLE_DEBUG) {
			printf("Result %ld\n", ret);
			printf("R_LEVEL %ld\n", R_LEVEL(ret));
			printf("R_SUMMARY %ld\n", R_SUMMARY(ret));
			printf("R_MODULE %ld\n", R_MODULE(ret));
			printf("R_DESCRIPTION %ld\n", R_DESCRIPTION(ret));
			printf("errno: %d\n", errno);
			printf("errno str: %s\n", strerror(errno));
		}
		failExit("could not move ROM to final path\n");
	}
}

// Returns true on successful connection
bool waitForWifi() {
	u32 wifi = 0;
	// Check if wifi is connected
	if (R_FAILED(ACU_GetWifiStatus(&wifi)) || !wifi) {
		printf(CONSOLE_YELLOW);
		printf("\nWaiting for wifi\n");
		printf(CONSOLE_RESET);
		printf("\nPress " CONSOLE_RED "Start" CONSOLE_RESET " to exit\n");
		while (!wifi && aptMainLoop()) {
			gspWaitForVBlank();
			hidScanInput();

			// Check if wifi is connected
			if (R_FAILED(ACU_GetWifiStatus(&wifi)) || !wifi) {
				wifi = 0;
			}

			u32 kDown = hidKeysDown();
			if (kDown & KEY_START) return false;
		}
	}
	printf(CONSOLE_GREEN);
	printf("\nWifi connected\n");
	printf(CONSOLE_RESET);
	return true;
}

void displayIpAddress() {
	struct in_addr ip_addr;
	struct in_addr netmask_addr;
	struct in_addr broadcast_addr;
	int ret = SOCU_GetIPInfo(&ip_addr, &netmask_addr, &broadcast_addr);
	if (ret) {
		failExit("could not get IP info\n");
	}
	printf("IP address: %s\n", inet_ntoa(ip_addr));
}

void socShutdown() {
	printf("Waiting for socExit...\n");
	socExit();
}

void allocateSocketBuffers() {
	TCP_buffer = malloc(TCP_BUFFER_SIZE);
	if (TCP_buffer == NULL) {
		failExit("failed to allocate buffer for TCP socket\n");
	}
	memset(TCP_buffer, 0, TCP_BUFFER_SIZE);

	UDP_buffer = malloc(UDP_BUFFER_SIZE);
	if (UDP_buffer == NULL) {
		failExit("failed to allocate buffer for UDP socket\n");
	}
	memset(UDP_buffer, 0, UDP_BUFFER_SIZE);
}

void freeSocketBuffers() {
	if (TCP_buffer != NULL) free(TCP_buffer);
	if (UDP_buffer != NULL) free(UDP_buffer);
}

void closeSockets() {
	if (tcp_listener >= 0) {
		close(tcp_listener);
		tcp_listener = -1;
	}
	if (tcp_downloader >= 0) {
		close(tcp_downloader);
		tcp_downloader = -1;
	}
	if (udp_listener >= 0) {
		close(udp_listener);
		udp_listener = -1;
	}
}

void setUpTcpListener() {
	int ret = 0;

	// Create TCP socket
	tcp_listener = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_listener < 0) {
		failExit("could not create tcp socket\n");
	}
	struct sockaddr_in tcp_listen_addr;
	memset(&tcp_listen_addr, 0, sizeof(tcp_listen_addr));
	tcp_listen_addr.sin_family = AF_INET;
	tcp_listen_addr.sin_port = htons(PORT);
	tcp_listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind TCP socket to listen for broadcasts
	ret = bind(tcp_listener, (struct sockaddr*) &tcp_listen_addr, sizeof(tcp_listen_addr));
	if (ret) {
		failExit("could not bind tcp socket\n");
	}

	// Set TCP socket to non-blocking so we can read input to exit
	fcntl(tcp_listener, F_SETFL, fcntl(tcp_listener, F_GETFL, 0) | O_NONBLOCK);

	// Start listening on TCP socket
	ret = listen(tcp_listener, 5);
	if (ret) {
		failExit("could not listen on tcp socket\n");
	}
}

void setUpUdpListener() {
	int ret = 0;

	// Create UDP socket
	udp_listener = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_listener < 0) {
		failExit("could not create udp socket\n");
	}
	struct sockaddr_in udp_listen_addr;
	memset(&udp_listen_addr, 0, sizeof(udp_listen_addr));
	udp_listen_addr.sin_family = AF_INET;
	udp_listen_addr.sin_port = htons(PORT);
	udp_listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind UDP socket to listen for broadcasts
	ret = bind(udp_listener, (struct sockaddr*) &udp_listen_addr, sizeof(udp_listen_addr));
	if (ret) {
		failExit("could not bind udp socket\n");
	}

	// Set UDP socket to non-blocking so we can read input to exit
	fcntl(udp_listener, F_SETFL, fcntl(udp_listener, F_GETFL, 0) | O_NONBLOCK);
}

void checkUdpBroadcast(struct sockaddr_in* addr) {
	u32 addr_len = sizeof(*addr);
	int recv_len = recvfrom(udp_listener, UDP_buffer, UDP_BUFFER_SIZE, 0, (struct sockaddr*) addr, &addr_len);
	if (recv_len == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		printf("errno: %d\n", errno);
		printf("errno str: %s\n", strerror(errno));
		failExit("could not receive from udp socket\n");
	}
	if (recv_len == -1) {
		return;
	}

	if (strncmp((const char*) UDP_buffer, INIT_REQUEST, strlen(INIT_REQUEST))) {
		return;
	}
	printf("\nReceived init request\n");

	printf("Acknowledging request\n");
	int send_len = sendto(udp_listener, INIT_RESPONSE, strlen(INIT_RESPONSE), 0, (const struct sockaddr*) addr, addr_len);
	if (send_len == -1) {
		failExit("could not respond to udp broadcast\n");
	}
}

// Returns true if we're still waiting for more bytes
bool checkTcpSocket(struct sockaddr_in* addr) {
	u32 addr_len = sizeof(*addr);
	if (tcp_downloader < 0) {
		tcp_downloader = accept(tcp_listener, (struct sockaddr*) addr, &addr_len);
		if (tcp_downloader >= 0) {
			printf("\nConnecting to port %d from %s\n", addr->sin_port, inet_ntoa(addr->sin_addr));
		}
	}
	if (tcp_downloader < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		printf("errno: %d\n", errno);
		printf("errno str: %s\n", strerror(errno));
		failExit("could not accept connection from tcp socket\n");
	}
	if (tcp_downloader < 0) {
		// No connections to accept
		return true;
	}

	s32 recv_len = recv(tcp_downloader, TCP_buffer + TCP_buffer_offset, TCP_BUFFER_SIZE - TCP_buffer_offset, 0);
	if (recv_len == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		printf("errno: %d\n", errno);
		printf("errno str: %s\n", strerror(errno));
		failExit("could not receive from tcp socket\n");
	}
	if (recv_len == -1) {
		// Nothing to receive
		return true;
	}

	// No point in writing to the file until we've filled up the buffer
	TCP_buffer_offset += recv_len;
	if (TCP_buffer_offset == TCP_BUFFER_SIZE || recv_len == 0) {
		openRom();
		printf(CONSOLE_MAGENTA);
		printf("Writing %ld bytes to file\n", TCP_buffer_offset);
		printf(CONSOLE_RESET);
		u32 num_bytes = 0;
		Result ret;
		ret = FSFILE_Write(rom_fd, &num_bytes, bytes_written, TCP_buffer, TCP_buffer_offset, FS_WRITE_FLUSH);
		if (R_FAILED(ret)) {
			failExit("could not write to file\n");
		}
		closeRom();
		if (num_bytes != TCP_buffer_offset) {
			failExit("could not write all bytes to file\n");
		}
		bytes_written += num_bytes;
		TCP_buffer_offset = 0;
	}

	file_size += recv_len;
	printf("Receiving ROM: %ld bytes\r", file_size);

	if (recv_len == 0 || file_size >= MAX_ROM_SIZE) {
		// We've received everything
		printf(CONSOLE_GREEN);
		printf("\x1b[2K\rROM transfer complete\n");
		printf(CONSOLE_RESET);
		printf("ROM size: %ld bytes\n", file_size);
		return false;
	}

	return true;
}

int main(int argc, char** argv) {
	int ret = 0;

	initSdmcStructs();
	atexit(deinitSdmcStructs);

	atexit(closeSockets);

	gfxInitDefault();
	atexit(gfxExit);

	consoleInit(GFX_TOP, NULL);

	printf(CONSOLE_GREEN);
	printf("%s\n\n", APP_TITLE);
	printf(CONSOLE_RESET);

	// Make sure the firm buffer is at the beginning of FCRAM
	checkFirmAddress();

	// Make sure we close the ROM before exiting
	atexit(closeRom);

	// Wait for wifi
	bool should_continue = waitForWifi();
	if (!should_continue) {
		printf("\nExiting...\n");
		for (int i = 0; i < 60; i++) gspWaitForVBlank();
		return 0;
	}

	// Allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFER_SIZE);
	if (SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	// Initialize soc:u service
	ret = socInit(SOC_buffer, SOC_BUFFER_SIZE);
	if (ret) {
		failExit("socInit: 0x%08x\n", (u32)ret);
	}
	atexit(socShutdown);

	// Display IP address
	displayIpAddress();

	// Allocate UDP and TCP buffers
	allocateSocketBuffers();
	atexit(freeSocketBuffers);

	setUpTcpListener();
	setUpUdpListener();

	printf("\nPress " CONSOLE_RED "Start" CONSOLE_RESET " to exit\n");
	printf("Press " CONSOLE_YELLOW "Select" CONSOLE_RESET " to reboot into open_agb_firm\n");
	printf("\n" CONSOLE_CYAN "Waiting for init packet" CONSOLE_RESET "\n");
	struct sockaddr_in udp_broadcaster_addr;
	struct sockaddr_in tcp_downloader_addr;
	bool waiting_for_file = true;
	bool should_reboot = false;
	bool download_complete = false;
	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		// We only need to check for broadcasts if we haven't received any bytes yet
		if (!file_size) {
			checkUdpBroadcast(&udp_broadcaster_addr);
		}

		if (waiting_for_file) {
			waiting_for_file = checkTcpSocket(&tcp_downloader_addr);
		}

		if (!waiting_for_file) {
			// Break out of loop so we can reboot into open_agb_firm
			should_reboot = true;
			// Assume download is complete
			download_complete = true;
			break;
		}

		u32 kDown = hidKeysDown();
		if (kDown & KEY_SELECT) {
			// Break out of loop so we can reboot into open_agb_firm
			should_reboot = true;
			break;
		}
		if (kDown & KEY_START) break;
	}

	if (download_complete) {
		moveRomToFinalPath();
	}

	if (ENABLE_DEBUG) {
		printf("\nPress Start to continue\n");
		while (aptMainLoop()) {
			gspWaitForVBlank();
			hidScanInput();
			u32 kDown = hidKeysDown();
			if (kDown & KEY_START) break;
		}
	}

	if (should_reboot) {
		// Load open_agb_firm into memory
		loadOafFirm();
		printf("\nRebooting into open_agb_firm...\n");
		for (int i = 0; i < 60; i++) gspWaitForVBlank();
		// Turn the wifi LED off
		MCUHWC_SetWifiLedState(false);
		APT_HardwareResetAsync();
	} else {
		printf("\nExiting...\n");
		for (int i = 0; i < 60; i++) gspWaitForVBlank();
	}

	return 0;
}

void failExit(const char* fmt, ...) {
	closeSockets();
	freeSocketBuffers();

	va_list ap;
	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}

void __attribute__((weak)) __appInit(void) {
	// Allocate before anything happens to make sure we get the right memory
	// offset for the firm buffer
	firm_buffer = linearAlloc(FIRM_OFFSET + FIRM_MAX_SIZE);

	// Initialize services
	srvInit();
	aptInit();
	acInit();
	hidInit();
	fsInit();
	mcuHwcInit();
	archiveMountSdmc();
}

void __attribute__((weak)) __appExit(void) {
	// Exit services
	archiveUnmountAll();
	mcuHwcExit();
	fsExit();
	hidExit();
	acExit();
	aptExit();
	srvExit();

	// Flush CPU cache into RAM before freeing firm buffer
	GSPGPU_FlushDataCache(firm_buffer, FIRM_MAX_SIZE);
	linearFree(firm_buffer);
}
