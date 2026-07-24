#!/usr/bin/env bash

# ESP-IDF 构建入口。可直接执行，也可用 source 执行 env 以切换当前终端。

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLOBAL_IDF_PATH="/home/xl/.espressif/v5.1.2/esp-idf"
LOCAL_IDF_PATH="${PROJECT_ROOT}/esp-idf"
IDF_CHANGED=0

show_usage()
{
    printf '%s\n' "用法:"
    printf '%s\n' "  source ./build.sh env          切换当前终端到项目内 esp-idf（缺失时使用全局 SDK）"
    printf '%s\n' "  ./build.sh build               优先使用项目内 esp-idf 编译"
    printf '%s\n' "  ./build.sh clean               清理 bootloader 的 SDK/CMake 缓存"
    printf '%s\n' "  ./build.sh flash /dev/ttyACM0  优先使用项目内 esp-idf 烧录"
}

select_idf_path()
{
    if [[ -f "${LOCAL_IDF_PATH}/export.sh" ]]; then
        printf '%s\n' "${LOCAL_IDF_PATH}"
    elif [[ -f "${GLOBAL_IDF_PATH}/export.sh" ]]; then
        printf '%s\n' "${GLOBAL_IDF_PATH}"
    else
        printf '错误: 未找到项目内或全局 ESP-IDF。\n' >&2
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
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            prepare_build_directory || return 1
            idf.py build
            ;;
        clean)
            clean_bootloader_cache
            ;;
        flash)
            if [[ $# -ne 2 ]]; then
                printf '%s\n' "错误: flash 需要指定串口，例如 ./build.sh flash /dev/ttyACM0" >&2
                return 1
            fi
            selected_idf_path="$(select_idf_path)" || return 1
            activate_idf "${selected_idf_path}" || return 1
            show_active_idf
            prepare_build_directory || return 1
            if [[ "${IDF_CHANGED}" -eq 1 ]]; then
                idf.py build || return 1
            fi
            idf.py -p "$2" flash
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
