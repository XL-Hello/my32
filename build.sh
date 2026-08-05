#!/usr/bin/env bash

# ESP-IDF 构建入口。可直接执行，也可用 source 执行 env 以切换当前终端。

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_IDF_PATH="${PROJECT_ROOT}/esp-idf"
DEFAULT_SERIAL_PORT="${DEFAULT_SERIAL_PORT:-/dev/ttyACM0}"
LITTLEFS_IMAGE_SOURCE_DIR="${PROJECT_ROOT}/app/ui/icon/png"
ALBUM_PHOTO_SOURCE_DIR="${PROJECT_ROOT}/app/ui/home/sub_home/album/photo"
LITTLEFS_FONT_SOURCE_FILE="${PROJECT_ROOT}/app/ui/front/Noto-Sans-SC-Bold/NotoSansSCMedium-4.ttf"
LITTLEFS_FONT_RAW_SYMBOLS_FILE="${PROJECT_ROOT}/app/ui/front/list_raw.txt"
LITTLEFS_FONT_SYMBOLS_FILE="${PROJECT_ROOT}/app/ui/front/list.txt"
LITTLEFS_FONT_LIST_GENERATOR="${PROJECT_ROOT}/app/ui/front/generate_font_list.py"
LITTLEFS_STAGE_DIR="${PROJECT_ROOT}/build/littlefs_assets"
LITTLEFS_IMAGE_FILE="${PROJECT_ROOT}/build/littlefs_icons.bin"
LITTLEFS_IMAGE_OFFSET="0x600000"
LITTLEFS_IMAGE_SIZE="0x200000"
LITTLEFS_BLOCK_SIZE="4096"
LITTLEFS_PAGE_SIZE="256"
MKLITTLEFS_ARCHIVE="${PROJECT_ROOT}/tools/mklittlefs/x86_64-linux-gnu-mklittlefs-42acb97.tar.gz"
MKLITTLEFS_BINARY="${PROJECT_ROOT}/build/mklittlefs"
SIZE_REPORT_FILE="${PROJECT_ROOT}/build/firmware-size-report.txt"
IDF_CHANGED=0

show_usage()
{
    printf '%s\n' "用法:"
    printf '%s\n' "  source ./build.sh env                    切换当前终端到项目内 esp-idf"
    printf '%s\n' "  ./build.sh build                         使用项目内 esp-idf 编译"
    printf '%s\n' "  ./build.sh build flash                   编译并打包 LittleFS 图标与字体资源"
    printf '%s\n' "  ./build.sh menuconfig                    使用项目内 esp-idf 打开配置界面"
    printf '%s\n' "  ./build.sh size                          输出固件内存、组件及源文件大小报告"
    printf '%s\n' "  ./build.sh clean               清理 bootloader 的 SDK/CMake 缓存"
    printf '%s\n' "  ./build.sh flash [串口]                  烧录应用，默认 ${DEFAULT_SERIAL_PORT}"
    printf '%s\n' "  ./build.sh flash flash [串口]            单独烧录 LittleFS 图标与字体资源，默认 ${DEFAULT_SERIAL_PORT}"
    printf '%s\n' "  可通过 DEFAULT_SERIAL_PORT 环境变量覆盖默认串口。"
}

generate_size_report()
{
    local report_dir="${PROJECT_ROOT}/build"
    local temp_dir
    local summary_file
    local components_file
    local files_file
    local report_temp_file

    mkdir -p "${report_dir}" || return 1
    temp_dir="$(mktemp -d "${report_dir}/size-report.XXXXXX")" || return 1
    summary_file="${temp_dir}/summary.txt"
    components_file="${temp_dir}/components.txt"
    files_file="${temp_dir}/files.txt"
    report_temp_file="${temp_dir}/firmware-size-report.txt"

    if ! idf.py size --format text --output-file "${summary_file}" ||
       ! idf.py size-components --format text --output-file "${components_file}" ||
       ! idf.py size-files --format text --output-file "${files_file}"; then
        rm -rf -- "${temp_dir}"
        return 1
    fi

    {
        printf '%s\n' "固件大小报告"
        printf '生成时间: %s\n' "$(date '+%Y-%m-%d %H:%M:%S %z')"
        printf '%s\n\n' "项目: ${PROJECT_ROOT}"

        printf '%s\n' "========== 1. 内存区占用（IRAM、DRAM/DIRAM、Flash） =========="
        sed -n '1,$p' "${summary_file}"
        printf '\n%s\n' "========== 2. 组件占用（size-components） =========="
        sed -n '1,$p' "${components_file}"
        printf '\n%s\n' "========== 3. 源文件占用（size-files） =========="
        sed -n '1,$p' "${files_file}"
    } > "${report_temp_file}" || {
        rm -rf -- "${temp_dir}"
        return 1
    }

    mv "${report_temp_file}" "${SIZE_REPORT_FILE}" || {
        rm -rf -- "${temp_dir}"
        return 1
    }
    rm -rf -- "${temp_dir}"
    printf '已生成固件大小报告: %s\n' "${SIZE_REPORT_FILE}"
}

select_idf_path()
{
    if [[ -f "${LOCAL_IDF_PATH}/export.sh" ]]; then
        printf '%s\n' "${LOCAL_IDF_PATH}"
    else
        printf '错误: 未找到项目内 ESP-IDF: %s\n' "${LOCAL_IDF_PATH}" >&2
        return 1
    fi
}

activate_idf()
{
    local idf_path="$1"

    if [[ ! -f "${idf_path}/export.sh" ]]; then
        printf '错误: 未找到 ESP-IDF: %s\n' "${idf_path}" >&2
        return 1
    fi

    export IDF_PATH="${idf_path}"
    # export.sh 会补充 idf.py、工具链和 Python 环境。
    source "${IDF_PATH}/export.sh" >/dev/null
}

show_active_idf()
{
    printf '当前使用的 ESP-IDF: %s\n' "${IDF_PATH}"
}

prepare_build_directory()
{
    local bootloader_command_file="${PROJECT_ROOT}/build/bootloader-prefix/tmp/bootloader-cfgcmd.txt"
    local config_env_file="${PROJECT_ROOT}/build/config.env"
    local bootloader_cache_file="${PROJECT_ROOT}/build/bootloader/CMakeCache.txt"
    local cached_idf_path
    local -a cached_idf_paths=()

    IDF_CHANGED=0
    if [[ -f "${bootloader_command_file}" ]]; then
        cached_idf_paths+=("$(rg -o -m 1 -- '-DIDF_PATH=[^;]+' "${bootloader_command_file}" 2>/dev/null | cut -d= -f2-)")
    fi
    if [[ -f "${config_env_file}" ]]; then
        cached_idf_paths+=("$(rg -o -m 1 '"IDF_PATH": "[^"]+"' "${config_env_file}" 2>/dev/null | cut -d'"' -f4)")
    fi
    if [[ -f "${bootloader_cache_file}" ]]; then
        cached_idf_paths+=("$(rg -m 1 '^IDF_PATH:.*=' "${bootloader_cache_file}" 2>/dev/null | cut -d= -f2-)")
    fi

    for cached_idf_path in "${cached_idf_paths[@]}"; do
        if [[ -n "${cached_idf_path}" && "${cached_idf_path}" != "${IDF_PATH}" ]]; then
            printf '检测到 ESP-IDF 已从 %s 切换到 %s，清理旧构建缓存。\n' \
                   "${cached_idf_path}" "${IDF_PATH}"
            idf.py fullclean || return 1
            IDF_CHANGED=1
            return 0
        fi
    done
}

clean_bootloader_cache()
{
    local bootloader_build_dir="${PROJECT_ROOT}/build/bootloader"
    local bootloader_prefix_dir="${PROJECT_ROOT}/build/bootloader-prefix"

    if [[ ! -d "${bootloader_build_dir}" && ! -d "${bootloader_prefix_dir}" ]]; then
        printf '%s\n' "未找到 bootloader CMake 缓存，无需清理。"
        return 0
    fi

    rm -rf -- "${bootloader_build_dir}" "${bootloader_prefix_dir}"
    printf '%s\n' "已清理 bootloader 的 SDK/CMake 缓存。"
}

ensure_mklittlefs()
{
    if [[ ! -f "${MKLITTLEFS_ARCHIVE}" ]]; then
        printf '错误: 未找到 mklittlefs 工具包: %s\n' "${MKLITTLEFS_ARCHIVE}" >&2
        return 1
    fi

    if [[ ! -x "${MKLITTLEFS_BINARY}" || "${MKLITTLEFS_ARCHIVE}" -nt "${MKLITTLEFS_BINARY}" ]]; then
        mkdir -p "${PROJECT_ROOT}/build" || return 1
        tar -xOf "${MKLITTLEFS_ARCHIVE}" mklittlefs/mklittlefs > "${MKLITTLEFS_BINARY}" || return 1
        chmod +x "${MKLITTLEFS_BINARY}" || return 1
    fi
}

build_littlefs_assets()
{
    if [[ ! -d "${LITTLEFS_IMAGE_SOURCE_DIR}" ]]; then
        printf '错误: 未找到 LittleFS 图标目录: %s\n' "${LITTLEFS_IMAGE_SOURCE_DIR}" >&2
        return 1
    fi

    if [[ ! -d "${ALBUM_PHOTO_SOURCE_DIR}" ]]; then
        printf '错误: 未找到相册 PNG 源目录: %s\n' "${ALBUM_PHOTO_SOURCE_DIR}" >&2
        return 1
    fi

    if [[ ! -f "${LITTLEFS_FONT_SOURCE_FILE}" ]]; then
        printf '错误: 未找到字体源文件: %s\n' "${LITTLEFS_FONT_SOURCE_FILE}" >&2
        return 1
    fi

    if [[ ! -f "${LITTLEFS_FONT_RAW_SYMBOLS_FILE}" ]]; then
        printf '错误: 未找到字体原始字符列表: %s\n' "${LITTLEFS_FONT_RAW_SYMBOLS_FILE}" >&2
        return 1
    fi

    if [[ ! -f "${LITTLEFS_FONT_LIST_GENERATOR}" ]]; then
        printf '错误: 未找到字体字符列表生成脚本: %s\n' "${LITTLEFS_FONT_LIST_GENERATOR}" >&2
        return 1
    fi

    if ! command -v lv_font_conv >/dev/null 2>&1; then
        printf '%s\n' "错误: 未安装 lv_font_conv；请执行 npm i lv_font_conv -g。" >&2
        return 1
    fi

    ensure_mklittlefs || return 1
    python3 "${LITTLEFS_FONT_LIST_GENERATOR}" || return 1
    rm -rf -- "${LITTLEFS_STAGE_DIR}" || return 1
    mkdir -p "${LITTLEFS_STAGE_DIR}/fonts" "${LITTLEFS_STAGE_DIR}/png" \
             "${LITTLEFS_STAGE_DIR}/photo" || return 1
    cp -R "${LITTLEFS_IMAGE_SOURCE_DIR}/." "${LITTLEFS_STAGE_DIR}/png" || return 1

    local photo_path
    local photo_count=0
    for photo_path in "${ALBUM_PHOTO_SOURCE_DIR}"/*.png; do
        [[ -f "${photo_path}" ]] || continue
        cp "${photo_path}" "${LITTLEFS_STAGE_DIR}/photo/" || return 1
        ((photo_count += 1))
    done
    printf '已打包相册 PNG：%d 个\n' "${photo_count}"

    local font_size
    local font_symbols
    font_symbols="$(<"${LITTLEFS_FONT_SYMBOLS_FILE}")"
    for font_size in 8 9 11 12 13 14 15 16 20; do
        lv_font_conv --bpp 4 --size "${font_size}" \
            --font "${LITTLEFS_FONT_SOURCE_FILE}" \
            --symbols "${font_symbols}" \
            --format bin --no-compress \
            --output "${LITTLEFS_STAGE_DIR}/fonts/esp_front_${font_size}.bin" || return 1
    done

    "${MKLITTLEFS_BINARY}" -c "${LITTLEFS_STAGE_DIR}" \
        -b "${LITTLEFS_BLOCK_SIZE}" \
        -p "${LITTLEFS_PAGE_SIZE}" \
        -s "${LITTLEFS_IMAGE_SIZE}" \
        "${LITTLEFS_IMAGE_FILE}" || return 1
    printf '已生成 LittleFS 图标与字体资源: %s\n' "${LITTLEFS_IMAGE_FILE}"
}

flash_littlefs_assets()
{
    local port="$1"

    if [[ ! -f "${LITTLEFS_IMAGE_FILE}" ]]; then
        printf '错误: 未找到 LittleFS 图标与字体资源: %s\n请先执行 ./build.sh build flash。\n' \
               "${LITTLEFS_IMAGE_FILE}" >&2
        return 1
    fi

    python -m esptool --chip esp32s3 --port "${port}" write_flash \
        "${LITTLEFS_IMAGE_OFFSET}" "${LITTLEFS_IMAGE_FILE}"
}

main()
{
    local command="${1:-}"
    local selected_idf_path

    case "${command}" in
        env)
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
                printf '%s\n' "提示: 直接运行脚本不会修改当前终端；请使用 source ./build.sh env。"
            fi
            ;;
        build)
            if [[ $# -gt 2 || ( $# -eq 2 && "$2" != "flash" ) ]]; then
                printf '%s\n' "错误: build 仅支持可选参数 flash。" >&2
                return 1
            fi
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            prepare_build_directory || return 1
            idf.py build || return 1
            if [[ "${2:-}" == "flash" ]]; then
                build_littlefs_assets
            fi
            ;;
        menuconfig)
            if [[ $# -ne 1 ]]; then
                printf '%s\n' "错误: menuconfig 不接受额外参数。" >&2
                return 1
            fi
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            prepare_build_directory || return 1
            idf.py menuconfig
            ;;
        size)
            if [[ $# -ne 1 ]]; then
                printf '%s\n' "错误: size 不接受额外参数。" >&2
                return 1
            fi
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            generate_size_report
            ;;
        clean)
            clean_bootloader_cache
            ;;
        flash)
            if [[ $# -gt 3 ]]; then
                printf '%s\n' "错误: flash 最多接受资源标识和一个串口参数。" >&2
                return 1
            fi
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            if [[ "${2:-}" == "flash" ]]; then
                flash_littlefs_assets "${3:-${DEFAULT_SERIAL_PORT}}"
            else
                if [[ $# -eq 3 ]]; then
                    printf '%s\n' "错误: 烧录应用时仅接受一个串口参数。" >&2
                    return 1
                fi
                prepare_build_directory || return 1
                if [[ "${IDF_CHANGED}" -eq 1 ]]; then
                    idf.py build || return 1
                fi
                idf.py -p "${2:-${DEFAULT_SERIAL_PORT}}" flash
            fi
            ;;
        -h|--help|help|"")
            show_usage
            ;;
        *)
            printf '错误: 不支持的参数: %s\n' "${command}" >&2
            show_usage >&2
            return 1
            ;;
    esac
}

main "$@"
status=$?

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    exit "${status}"
fi
return "${status}"
