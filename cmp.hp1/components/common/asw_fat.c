#include "asw_fat.h"
#include "diskio_wl.h" //  [tgl mark] for ff_diskio_register_wl_partition

// Mount path for the partition
char *data_path = "/inv";
char *base_path = "/home";

static const char *TAG = "asw_fat.c";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
sys_log net_log = {0};

//---------------------------------//

int force_format(char *partname);
//--------------------------//

//---------------------------------//
void make_lower_case(char *str, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        str[i] = tolower(str[i]);
    }
}

//-----------------------//
int force_format(char *partname)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partname);
    assert(partition != NULL);

    ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));
    ESP_LOGI(TAG, "######force format %s#######\n", partname);
    return 0;
}

//------------------------//

int get_file_size(char *path)
{
    FILE *fp = NULL;
    long lSize;

    fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    fseek(fp, 0, SEEK_END);
    lSize = ftell(fp);
    fclose(fp);

    return (int)lSize;
}
//-------------------------//

static void get_file_list_from_dir(char *path, cJSON *arr)
{
    struct dirent *next_file;
    char filepath[256] = {0};

    DIR *theFolder = opendir(path);
    while ((next_file = readdir(theFolder)) != NULL)
    {
        memset(filepath, 0, sizeof(filepath));
        if (strcmp(next_file->d_name, ".") == 0 || strcmp(next_file->d_name, "..") == 0)
            continue;
        // sprintf(filepath, "%s/%s \r\n", path, next_file->d_name);
        if (check_file_exist(filepath) < 0) //  [tgl mark] define in the cloud come in soon
            continue;

        int filesize = get_file_size(filepath);
        if (filesize > 0)
        {
            cJSON *item = cJSON_CreateObject();
            make_lower_case(next_file->d_name, strlen(next_file->d_name));
            cJSON_AddStringToObject(item, "file", next_file->d_name);
            cJSON_AddNumberToObject(item, "size", filesize);
            cJSON_AddItemToArray(arr, item);
        }
    }
    closedir(theFolder);
}

//--------------------------//
void get_file_list(char *msg)
{
    cJSON *res = cJSON_CreateObject();
    cJSON *myArr = NULL;
    char *json_str = NULL;

    myArr = cJSON_AddArrayToObject(res, "filelist");

    get_file_list_from_dir(base_path, myArr);
    // get_file_list_from_dir(data_path, myArr);
    json_str = cJSON_PrintUnformatted(res);
    memcpy(msg, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(res);
}

/*******************************************************/
static void clear_dir(char *path)
{
    // These are data types defined in the "dirent" header
    DIR *theFolder = opendir((const char *)path);
    struct dirent *next_file;
    char filepath[300];

    while ((next_file = readdir(theFolder)) != NULL)
    {
        // build the path for each file in the folder
        if (strcmp(next_file->d_name, ".") == 0 || strcmp(next_file->d_name, "..") == 0)
            continue;
        sprintf(filepath, "%s/%s", path, next_file->d_name);
        remove(filepath);
        ESP_LOGI(TAG, "removed: %s\n", filepath);
    }
    closedir(theFolder);
}
//--------------------------//

void clear_file_system(void)
{
    clear_dir(base_path);
    clear_dir(data_path);
}
//-----------------------------------------------------//
int8_t asw_fat_mount(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount storage FATFS (%s)", esp_err_to_name(err));
        return ASW_FAIL;
    }
    ESP_LOGI(TAG, "Mount storage FAT OK");
    return ASW_OK;
}

//-----------------------------------------------------//
int8_t inv_fat_mount(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    esp_err_t err = esp_vfs_fat_spiflash_mount(data_path, "invdata", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount invdata FATFS (%s)", esp_err_to_name(err));
        return ASW_FAIL;
    }
    ESP_LOGI(TAG, "Mount invdata FAT OK");
    return ASW_OK;
}
//-------------------------------------//
int get_sys_log(char *buf)
{

    memcpy(buf, net_log.tag, strlen(net_log.tag));
    memcpy(&buf[strlen(net_log.tag)], net_log.log, net_log.index);
    return 0;
}

//---------------------------------//
int check_file_exist(char *path)
{
    if (access(path, 0) != 0)
    {
        // ESP_LOGE(path, "not existed \n");
        return -1;
    }
    return 0;
}

//-------------------------------------//
int check_inv_pv(Inv_data *inv_data)
{
    int i = 0;
    for (i = 0; i < 10;)
        if (inv_data->PV_cur_voltg[i].iVol > 800 && inv_data->PV_cur_voltg[i].iVol < 65535)
            break;
        else
            i++;

    if (i > 9)
    {
        printf("PV TO LOW return \n");
        return -1;
    }
    ESP_LOGI(TAG, "PV > 800  %d\n", inv_data->PV_cur_voltg[i].iVol);
    return 0;
}
//------------------------------------//
esp_err_t esp_vfs_fat_spiflash_mount_force_format(const char *base_path,
                                                  const char *partition_label,
                                                  const esp_vfs_fat_mount_config_t *mount_config,
                                                  wl_handle_t *wl_handle)
{
    esp_err_t result = ESP_OK;
    const size_t workbuf_size = 4096;
    void *workbuf = NULL;

    esp_partition_subtype_t subtype = partition_label ? ESP_PARTITION_SUBTYPE_ANY : ESP_PARTITION_SUBTYPE_DATA_FAT;
    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                     subtype, partition_label);

    ESP_LOGI(TAG, "######esp_vfs_fat_spiflash_mount_force_format#######\n");
    if (data_partition == NULL)
    {
        // ESP_LOGE(TAG, "Failed to find FATFS partition (type='data', subtype='fat', partition_label='%s'). Check the partition table.", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    result = wl_mount(data_partition, wl_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to mount wear levelling layer. result = %i", result);
        return result;
    }
    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) //[tgl mark]ff_diskio 跟fats有關的 未找到具体的使用说明文档
    {
        ESP_LOGD(TAG, "the maximum count of volumes is already mounted");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    result = ff_diskio_register_wl_partition(pdrv, *wl_handle); //[tgl mark]找不到此函数的定义 在 component/fats/vfs/文件下 头文件 [https://github.com/jkearins/ESP32_mkfatfs]
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "ff_diskio_register_wl_partition failed pdrv=%i, error - 0x(%x)", pdrv, result);
        goto fail;
    }
    FATFS *fs;
    result = esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs);
    if (result == ESP_ERR_INVALID_STATE)
    {
        // it's okay, already registered with VFS
    }
    else if (result != ESP_OK)
    {
        ESP_LOGD(TAG, "esp_vfs_fat_spiflash_mount_force_format esp_vfs_fat_register failed 0x(%x)", result);
        goto fail;
    }

    FRESULT fresult = f_mount(fs, drv, 1);

    // ESP_LOGW(TAG, "esp_vfs_fat_spiflash_mount_force_format f_mount failed (%d)", fresult);
    workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL)
    {
        result = ESP_ERR_NO_MEM;
        goto fail;
    }
    size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
        CONFIG_WL_SECTOR_SIZE,
        mount_config->allocation_unit_size);

    ESP_LOGW(TAG, "current partition %s\n", partition_label);
    ESP_LOGI(TAG, "esp_vfs_fat_spiflash_mount_force_format FATFS partition, allocation unit size=%d", alloc_unit_size);
    fresult = f_mkfs(drv, FM_ANY | FM_SFD, alloc_unit_size, workbuf, workbuf_size);
    if (fresult != FR_OK)
    {
        result = ESP_FAIL;
        ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
        goto fail;
    }
    free(workbuf);
    workbuf = NULL;
    ESP_LOGI(TAG, "esp_vfs_fat_spiflash_mount_force_format Mounting again");
    fresult = f_mount(fs, drv, 0);
    if (fresult != FR_OK)
    {
        result = ESP_FAIL;
        ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
        goto fail;
    }
    return ESP_OK;

fail:
    free(workbuf);
    esp_vfs_fat_unregister_path(base_path);
    ff_diskio_unregister(pdrv);
    return result;
}

//=============================//
int force_flash_reformart(char *partion)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};

    esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle);
    esp_err_t err = esp_vfs_fat_spiflash_mount_force_format(base_path, partion, &mount_config, &s_wl_handle);
    ESP_LOGI(TAG, "force_flash_reformart %s \n", partion);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_vfs_fat_spiflash_mount_force_format Failed to mount storage FATFS (%s)", esp_err_to_name(err));
        force_format(partion);
    }
    ESP_LOGI(TAG, "Mount storage FAT OK");
    return 0;
}

/***************************************/

int write_lost_data(Inv_data *inv_data)
{
    if (check_inv_pv(inv_data) < 0)
        return -1;

    FILE *fp = fopen("/inv/lost.db", "ab+"); //"wb"
    if (fp == NULL)
    {
        ESP_LOGE("ff", "Failed to open file for writing");
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    ESP_LOGE(TAG, "file len  %ld \n", ftell(fp));

    if (ftell(fp) > 4000000) // 5 invs 7day 1827000  ////1827000)
    {
        ESP_LOGE("write lostdata", "file too large move it");
        fclose(fp);
        transfer_another_file();

        FILE *fp = fopen("/inv/lost.db", "ab+"); //"wb"
        if (fp == NULL)
        {
            ESP_LOGE("ff", "Failed to open file for writing");
            return -3;
        }
    }

    fseek(fp, 0, SEEK_SET);

    uint8_t buff[300] = {0};
    memcpy(buff, (char *)inv_data, sizeof(Inv_data));

    uint16_t crc = crc16_calc(buff, sizeof(Inv_data));
    buff[sizeof(Inv_data)] = crc & 0xFF;
    buff[sizeof(Inv_data) + 1] = (crc >> 8) & 0xFF;
    // int i = 0;

    fwrite(buff, sizeof(char), sizeof(Inv_data) + 2, fp);

    fclose(fp);
    return 0;
}

//--------------------------------//
int write_lost_index(int index)
{
    if ((-1 == check_file_exist("/inv/lost.db")) || (index < 1))
        return -1;

    if (access("/inv/index.db", 0) != 0)
    {
        ESP_LOGE("write_lost_index", "index.db not existed \n");
        if (index > 1)
            return -1;
    }

    FILE *fp = fopen("/inv/index.db", "wb"); //"wb"
    if (fp == NULL)
    {
        ESP_LOGE("ff", "Failed to open file for writing");
        return -1;
    }
    char buff[32] = {0};

    sprintf(buff, "%d", index);
    ESP_LOGE("write index", "all write*%d#%s#--- ", index, buff);
    fwrite(buff, sizeof(char), sizeof(buff), fp);
    fclose(fp);
    return 0;
}

//--------------------------------//
int check_lost_data(void)
{
    if (access("/inv/lost.db", 0) != 0)
    {
        // ESP_LOGE("check_lost_data", "file don't existed \n");
        return -1;
    }

    FILE *fp = fopen("/inv/lost.db", "rb");
    if (fp == NULL)
    {
        ESP_LOGE("check_lost_data", "Failed to open file for reading");
        return -2;
    }

    uint8_t buff[300] = {0};
    fread(buff, sizeof(char), sizeof(Inv_data) + 2, fp);
    fclose(fp);

    if (0 != crc16_calc(buff, sizeof(Inv_data) + 2))
    {
        printf("check lost data crc error \n");
        return -3;
    }

    return 0;
}
//--------------------------------------//
int read_lost_data(Inv_data *inv_data, int *line)
{
    if (access("/inv/lost.db", 0) != 0)
    {
        // ESP_LOGE("read_lost_data", "file don't existed \n");
        return -1;
    }

    if (*line < 0)
        *line = 0;

    FILE *fp = fopen("/inv/lost.db", "rb");
    if (fp == NULL)
    {
        ESP_LOGE("TAG", "Failed to open file for reading");
        // remove("/inv/lost.db");
        return -1;
    }
    uint8_t buff[300] = {0};
    int tmp_line = 0;
    tmp_line = *line;

    fseek(fp, (*line) * 290, SEEK_SET);

    // while (!feof(fp) )
    {
        tmp_line++;
        fread(buff, sizeof(char), sizeof(Inv_data) + 2, fp);
        ESP_LOGE("TAG", "all read--%d--%d--  %s ", tmp_line, *line, (char *)buff);

        if (tmp_line > *line)
        {
            ESP_LOGE("TAG", "find new line --%d--%d--  %s ", tmp_line, *line, (char *)buff);
            // break;
        }
    }
    // if(tmp_line == *line)
    //     {
    //         ESP_LOGE("TAG", "file read to end--%d--%d--  %s ", tmp_line, *line, buff);
    //         break;
    //     }
    if (feof(fp))
    {
        ESP_LOGE("read lost data", "read file end delete file------------>>>>>>\n");
        remove("/inv/lost.db");
        remove("/inv/index.db");
        // remove("/inv/*");
    }
    fclose(fp);

    int i = 0;
    printf("read put invdata %d crc=%d \n", sizeof(Inv_data) + 2, crc16_calc(buff, sizeof(Inv_data) + 2));

    if (0 != crc16_calc(buff, sizeof(Inv_data) + 2))
    {
        printf("read lost data crc error \n");
        *line = tmp_line;
        return -2;
    }

    for (; i < sizeof(Inv_data) + 2; i++)
        printf("%02x ", buff[i]);
    printf("readput end\n");

    memcpy(inv_data, buff, sizeof(Inv_data));
    *line = tmp_line;
    ESP_LOGE("TAG", "read %d 'st line  invdata time %s %s\n", *line, inv_data->time, inv_data->psn);
    return 0;
}

/***************************************/
//------------------------------//
int read_lost_index(void)
{
    int tmp_index = 0;

    if (access("/inv/index.db", 0) != 0)
    {
        // ESP_LOGE("read_index_data", "file don't existed \n");
        return -1;
    }

    FILE *fp = fopen("/inv/index.db", "rb");
    if (fp == NULL)
    {
        ESP_LOGE("read_lost_index", "Failed to open file for readindex");
        return 0;
    }

    char buff[32] = {0};
    char i = 0;
    i = fread(buff, sizeof(char), 32, fp);
    ESP_LOGE("read index", "all read*%d#%s#--- ", i, buff);
    fclose(fp);
    
    tmp_index = atoi(buff);
    ESP_LOGE("read index", "read index is %d \n", tmp_index);
    if (tmp_index < 8000)
        return tmp_index;
    else
        return 0;
}

//------------------------------//

int find_new_days(FILE *fp, char *day)
{
    if (fp == NULL)
    {
        ESP_LOGE("read_lost_data", "find days error \n");
        return -1;
    }
    uint8_t buff[300] = {0};
    int i = 0;

    while (!feof(fp))
    {
        fread(buff, sizeof(char), sizeof(Inv_data) + 2, fp);
        ESP_LOGE("read index", "all read*%d#%s#--- ", i, buff);
        i++;

        if (0 != crc16_calc(buff, sizeof(Inv_data) + 2))
        {
            ESP_LOGI(TAG, "read lost data crc error \n");
            continue;
        }

        memcpy(day, &buff[50], 10);
        break;
    }

    ESP_LOGE("read days", "current available day %s line is %d \n", day, i);
    if (i)
        return i;
    return -1;
}
//----------------------------------
int find_next_day(FILE *fp, char *day, int lines)
{
    if (fp == NULL)
    {
        ESP_LOGE("read_lost_data", "find next days error \n");
        return -1;
    }
    uint8_t buff[300] = {0};
    int i = lines;

    while (!feof(fp))
    {
        fread(buff, sizeof(char), sizeof(Inv_data) + 2, fp);
        ESP_LOGE("read next days", "all read*%d#%s#--- ", i, buff);
        i++;

        if (0 != crc16_calc(buff, sizeof(Inv_data) + 2))
        {
            ESP_LOGE(TAG, "read lost data crc error \n");
            continue;
        }

        ESP_LOGE("find available next days", "it's %s \n", &buff[50]);
        if (strncmp((const char *)day, (char *)&buff[50], 10) != 0)
        {
            ESP_LOGE("read days", "next change day %s line is %d \n", day, i);
            return i;
        }
    }

    return -1;
}

EXT_RAM_ATTR char exram_buf[5900] = {0}; //[tgl mem]add
//--------------------------------
int transfer_another_file(void)
{
    FILE *new_fp = NULL;
    int nread = 0;
    int blob_size = 0;
    //  char buf[5900] = {0};
    int read_index = 0;
    char days[10] = {0};
    int line = 0;

    FILE *fp = fopen(OLD_DB_PATH, "rb");
    if (fp == NULL)
    {
        ESP_LOGE("read_lost_data", "Failed to open file for lostdata");
        return -1;
    }

    blob_size = sizeof(Inv_data) + 2;

    line = find_new_days(fp, days);
    if (line > 0)
    {
        fseek(fp, blob_size * line, SEEK_SET);
        line = find_next_day(fp, days, line);
        if (line < 2)
        {
            fclose(fp);
            remove(OLD_DB_PATH);
            remove("/inv/index.db");
            ESP_LOGE("move data", "all data repeat delet lost.db \n");
            return -1;
        }
    }
    else
    {
        fclose(fp);
        remove(OLD_DB_PATH);
        remove("/inv/index.db");
        ESP_LOGE("move data", "not found available day delet lost.db\n");
        return -1;
    }
    // ESP_LOGE("transfer_another_file ", "reseek line  %d \n",  line);
    if (fp != NULL)
    {
        fseek(fp, blob_size * line, SEEK_SET);
        new_fp = fopen(NEW_DB_PATH, "wb");
        while (!feof(fp)) // to read file
        {
            nread = fread(exram_buf, 1, blob_size * 20, fp);
            ESP_LOGE("move data", "read bytes %d \n", nread);

            if (0 == (nread % blob_size))
            {
                fwrite(exram_buf, 1, nread, new_fp);
            }
            usleep(10000);
        }
        fclose(fp);
        fclose(new_fp);
        remove(OLD_DB_PATH);
        rename(NEW_DB_PATH, OLD_DB_PATH);
        read_index = read_lost_index();
        // ESP_LOGE("transfer_another_file ", "redefine index %d %d \n", read_index, line);
        if (read_index > line)
        {
            write_lost_index(read_index - line);
        }
        else
        {
            remove("/inv/index.db");
        }
    }
    return 0;
}

//-------------------------------//
int network_log(char *msg)
{
    char now_time[30] = {0};
    char ip[30] = {0};
    char buf[300] = {0};

    get_time(now_time, sizeof(now_time));
    if (g_stick_run_mode == Work_Mode_AP_PROV)
        sprintf(buf, "%s ip:%s %s\r\n", now_time, "160.190.0.1", msg);
    else if (g_stick_run_mode == Work_Mode_LAN)
    {
        get_eth_ip(ip);
        sprintf(buf, "%s ip:%s %s\r\n", now_time, ip, msg);
    }
    else if (g_stick_run_mode == Work_Mode_STA)
    {
        get_sta_ip(ip);
        sprintf(buf, "%s ip:%s %s\r\n", now_time, ip, msg);
    }

    if (strlen(net_log.tag) == 0)
        memcpy(net_log.tag, "Internet info\r\n", strlen("Internet info\r\n"));
    // printf("write_log_enab#######````````````` %s %d %d %d\n", net_log.tag, net_log.index+ strlen(buf), net_log.index,strlen(buf));
    if (net_log.index + strlen(buf) > 329)
    {
        char tmp[160] = {0};

        if (net_log.index > 260)
            memcpy(tmp, &net_log.log[100], 160);
        else if (net_log.index > 100 && net_log.index < 261)
            memcpy(tmp, &net_log.log[100], net_log.index - 100);

        memset(net_log.log, 0, sizeof(net_log.log));
        net_log.index = 0;

        memcpy(net_log.log, tmp, strlen(tmp));
        net_log.log[160] = '\n';
        memcpy(&net_log.log[161], buf, strlen(buf));

        net_log.index += (161 + strlen(buf));
        // printf("========================net_log new index---- %d \n", net_log.index);
    }
    else
    {
        memcpy(&net_log.log[net_log.index], buf, strlen(buf));
        net_log.index += strlen(buf);
    }

    if (net_log.index > 330)
    {
        memset(net_log.log, 0, sizeof(net_log.log));
        net_log.index = 0;
    }
    return 0;
}
