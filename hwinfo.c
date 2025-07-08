#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

void print_cpu_info()
{
    printf("== CPU Info ==\n");

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        perror("  Failed to open /proc/cpuinfo");
        return;
    }

    char line[256];
    int cores = 0;
    char model[256] = "";

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) cores++;
        if (strncmp(line, "model name", 10) == 0 && model[0] == '\0') {
            char *val = strchr(line, ':');
            if (val) strncpy(model, val + 2, sizeof(model) - 1);
        }
    }

    fclose(fp);

    printf("  Model: %s", model[0] ? model : "Unknown\n");
    printf("  Cores: %d\n", cores);
}

void print_memory_info()
{
    printf("\n== Memory Info ==\n");

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("  Failed to open /proc/meminfo");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal", 8) == 0) {
            printf("  %s", line);
            break;
        }
    }

    fclose(fp);
}

void print_gpio_info()
{
    printf("\n== GPIOs ==\n");

    DIR *dir = opendir("/sys/class/gpio");
    if (!dir) {
        printf("  GPIO interface not available.\n");
        return;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "gpio", 4) == 0) {
            printf("  %s\n", entry->d_name);
            found = 1;
        }
    }

    if (!found)
        printf("  No GPIOs exported.\n");

    closedir(dir);
}

void print_i2c_info()
{
    printf("\n== I2C Buses and Devices ==\n");

    DIR *dir = opendir("/sys/class/i2c-dev");
    if (!dir) {
        printf("  I2C not available.\n");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_LNK && strncmp(entry->d_name, "i2c-", 4) == 0) {
            char bus_path[1024];
            snprintf(bus_path, sizeof(bus_path), "/sys/class/i2c-dev/%s/device", entry->d_name);

            DIR *bus_dir = opendir(bus_path);
            if (!bus_dir) continue;

            printf("  %s:\n", entry->d_name);
            struct dirent *dev;

            int found = 0;
            while ((dev = readdir(bus_dir)) != NULL) {
                // Look for i2c-X-Y address entries (like "1-0040")
                if (strchr(dev->d_name, '-') && dev->d_name[0] != '.') {
                    printf("    Device: %s\n", dev->d_name);
                    found = 1;
                }
            }

            if (!found)
                printf("    No devices found\n");

            closedir(bus_dir);
        }
    }

    closedir(dir);
}

void print_spi_info()
{
    printf("\n== SPI Controllers ==\n");

    DIR *dir = opendir("/sys/class/spi_master");
    if (!dir) {
        printf("  SPI not available.\n");
        return;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            printf("  %s\n", entry->d_name);
            found = 1;
        }
    }

    if (!found)
        printf("  No SPI controllers found.\n");

    closedir(dir);
}

void print_serial_info()
{
    printf("\n== Physical UART Devices ==\n");

    FILE *fp = fopen("/proc/tty/driver/serial", "r");
    if (fp) {
        char line[256];
        int found = 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "uart:") && !strstr(line, "unknown") &&
                !strstr(line, "virtual") && !strstr(line, "usb")) {
                printf("  %s", line);
                found = 1;
            }
        }

        fclose(fp);
        if (found) return;
    }

    // Fallback: look in /sys/class/tty/*/device
    DIR *dir = opendir("/sys/class/tty");
    if (!dir) {
        printf("  Unable to open /sys/class/tty\n");
        return;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        // We are mostly interested in ttyS*, but others like ttyAMA*, ttyO*, ttyMXC* are valid too
        if (strncmp(entry->d_name, "tty", 3) != 0)
            continue;

        char path[512];
        snprintf(path, sizeof(path), "/sys/class/tty/%s/device", entry->d_name);

        struct stat st;
        if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
            printf("  /dev/%s\n", entry->d_name);
            found = 1;
        }
    }

    closedir(dir);

    if (!found)
        printf("  No physical UARTs found.\n");
}

void print_input_devices()
{
    printf("\n== Input Devices ==\n");

    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        printf("  Could not open /proc/bus/input/devices\n");
        return;
    }

    char line[256];
    char name[256] = "";
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "N: Name=", 8) == 0) {
            char *start = strchr(line, '"');
            if (start) {
                strncpy(name, start + 1, sizeof(name) - 1);
                name[strcspn(name, "\"")] = '\0';
            }
        }

        if (line[0] == '\n' && name[0]) {
            printf("  %s\n", name);
            name[0] = '\0';
            found = 1;
        }
    }

    if (!found)
        printf("  No input devices found.\n");

    fclose(fp);
}

void print_rtc_info()
{
    printf("\n== RTC Devices ==\n");

    DIR *dir = opendir("/sys/class/rtc");
    if (!dir) {
        printf("  RTC interface not available.\n");
        return;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "rtc", 3) == 0 && entry->d_name[3] >= '0') {
            char name_path[1024];
            snprintf(name_path, sizeof(name_path), "/sys/class/rtc/%s/name", entry->d_name);

            FILE *fp = fopen(name_path, "r");
            if (fp) {
                char name[128];
                if (fgets(name, sizeof(name), fp)) {
                    name[strcspn(name, "\n")] = '\0';
                    printf("  %s (%s)\n", entry->d_name, name);
                }
                fclose(fp);
                found = 1;
            }
        }
    }

    if (!found)
        printf("  No RTCs found.\n");

    closedir(dir);
}

int main()
{
    printf("Hardware Information Tool V1.0\n");
    printf("=========================\n");

    print_cpu_info();
    print_memory_info();
    print_gpio_info();
    print_i2c_info();
    print_spi_info();
    print_serial_info();
    print_input_devices();
    print_rtc_info();

    return 0;
}

