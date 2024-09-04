#!/bin/sh

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# This script is used to install the RapidStream software on the target machine.

# If the user runs this script with the root privilege, it will install the software
# in the /opt/rapidstream-tapa directory. It further creates symbolic links in the
# /usr/local/bin directory to the executables in the /opt/rapidstream-tapa directory to
# make the software available in the system path.

# Otherwise, if the user runs this script without the root privilege, it will install
# the software in the $HOME/.rapidstream-tapa directory. It further modifies the user's
# PATH environment variable to include the $HOME/.rapidstream-tapa directory.

# Treat unset variables as an error when substituting. And exit immediately if a
# pipeline returns non-zero status.
set -ue

# Default values for the installation options.
RAPIDSTREAM_UPDATE_ROOT="${RAPIDSTREAM_UPDATE_ROOT:-https://releases.rapidstream-da.com}"
RAPIDSTREAM_LOCAL_PACKAGE="${RAPIDSTREAM_LOCAL_PACKAGE:-}"

RAPIDSTREAM_CHANNEL="${RAPIDSTREAM_CHANNEL:-stable}"
RAPIDSTREAM_VERSION="${RAPIDSTREAM_VERSION:-latest}"

if [ "$(id -u)" -eq 0 ]; then
  # Default to /opt/rapidstream-tapa if the user has the root privilege.
  RAPIDSTREAM_INSTALL_DIR="${RAPIDSTREAM_INSTALL_DIR:-/opt/rapidstream-tapa}"
  CREATE_SYMLINKS="yes"
  MODIFY_PROFILE_PATH="no"

elif [ "$(id -u)" -ne 0 ]; then
  # Default to the user's home directory if the user does not have the root privilege.
  RAPIDSTREAM_INSTALL_DIR="${RAPIDSTREAM_INSTALL_DIR:-$HOME/.rapidstream-tapa}"
  CREATE_SYMLINKS="no"
  MODIFY_PROFILE_PATH="yes"

fi

VERBOSE="${VERBOSE:-no}"
QUIET="${QUIET:-no}"

# Display the usage of this script.
usage() {
  cat <<EOF
tapa.rapidstream.sh - Installer of the RapidStream TAPA software on the target machine.

Usage: tapa.rapidstream.sh [OPTIONS]

Options:
  -c, --channel <channel>      Specify the channel to download the software from.
      --version <version>      Specify the version of the software to install.

  -t, --target <directory>     Specify the directory to install the software to.
      --no-create-symlinks     Do not create symbolic links in the system path.
      --no-modify-path         Do not modify the PATH environment variable.

  -v, --verbose                Enable verbose output.
  -q, --quiet                  Disable progress output.

  -h, --help                   Display this help message and exit.
EOF
}

# Main function of this script.
main() {
  # Parse the command-line arguments.
  parse_args "$@"

  # Display the installation options if the verbose mode is enabled.
  if [ "$VERBOSE" = "yes" ]; then
    echo "Please verify the specified installation options:"
    if [ -n "$RAPIDSTREAM_LOCAL_PACKAGE" ]; then
      echo "  Local package:     $RAPIDSTREAM_LOCAL_PACKAGE"
    else
      echo "  Update root:       $RAPIDSTREAM_UPDATE_ROOT"
      echo "  Channel:           $RAPIDSTREAM_CHANNEL"
      echo "  Version:           $RAPIDSTREAM_VERSION"
    fi
    echo "  Install target:    $RAPIDSTREAM_INSTALL_DIR"
    echo "  Create symlinks:   $CREATE_SYMLINKS"
    echo "  Modify PATH:       $MODIFY_PROFILE_PATH"
    printf "Press Enter to continue, or Ctrl+C to cancel..."
    read -r _
  fi

  # Check if the installation directory exists.
  check_install_dir

  # Display the installation message.
  if [ "$QUIET" = "no" ]; then
    echo "Installing RapidStream TAPA to \"$RAPIDSTREAM_INSTALL_DIR\"..."
  fi

  # Download and extract the RapidStream TAPA software.
  download_and_extract_rapidstream_tapa

  # Create symbolic links in the system path.
  create_symlinks

  # Modify the PATH environment variable.
  modify_profile_path
}

# Parse the command-line arguments.
parse_args() {
  while [ $# -gt 0 ]; do
    case "$1" in
    -c | --channel)
      RAPIDSTREAM_CHANNEL="$2"
      shift 2
      ;;
    --channel=*)
      RAPIDSTREAM_CHANNEL="${1#*=}"
      shift
      ;;
    --version)
      RAPIDSTREAM_VERSION="$2"
      shift 2
      ;;
    --version=*)
      RAPIDSTREAM_VERSION="${1#*=}"
      shift
      ;;
    -t | --target)
      RAPIDSTREAM_INSTALL_DIR="$2"
      shift 2
      ;;
    --target=*)
      RAPIDSTREAM_INSTALL_DIR="${1#*=}"
      shift
      ;;
    --no-create-symlinks)
      CREATE_SYMLINKS="no"
      shift
      ;;
    --no-modify-path)
      MODIFY_PROFILE_PATH="no"
      shift
      ;;
    -v | --verbose)
      VERBOSE="yes"
      shift
      ;;
    -q | --quiet)
      QUIET="yes"
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
    esac
  done

  # Verify the options.
  if [ "$VERBOSE" = "yes" ] && [ "$QUIET" = "yes" ]; then
    echo "Error: The options '-v' and '-q' cannot be used together."
    exit 1
  fi
}

# Check if the installation directory exists. If so, prompt the user to confirm.
check_install_dir() {
  # If the installation directory exists
  if [ -d "$RAPIDSTREAM_INSTALL_DIR" ]; then

    # If the user does not enable the auto-confirm option
    if [ "$VERBOSE" = "yes" ]; then
      # Prompt the user to confirm
      printf "The installation directory already exists. Do you want to overwrite it? [y/N]: "
      read -r answer

      # If the user does not confirm
      if [ "$answer" != "y" ] && [ "$answer" != "Y" ]; then
        # Abort the installation
        echo "Aborted. No changes were made."
        exit 1
      fi
    fi

    # If the user enables the auto-confirm option or confirms the prompt,
    # show the message that the installation directory will be overwritten.
    if [ "$QUIET" = "no" ]; then
      echo "Overwriting the installation directory: \"$RAPIDSTREAM_INSTALL_DIR\"..."
    fi
  fi
}

# Download and extract the RapidStream TAPA software.
download_and_extract_rapidstream_tapa() {
  # Create a temporary directory to download the RapidStream TAPA software.
  tmpdir="$(mktemp -d)"
  if [ "$VERBOSE" = "yes" ]; then
    echo "Creating a temporary directory for the download: \"$tmpdir\"..."
  fi

  if [ -f "$RAPIDSTREAM_LOCAL_PACKAGE" ]; then
    if [ "$VERBOSE" = "yes" ]; then
      echo "Copying RapidStream TAPA from: \"$RAPIDSTREAM_LOCAL_PACKAGE\" to \"$tmpdir/rapidstream-tapa.tar.gz\"..."
    fi
    cp "$RAPIDSTREAM_LOCAL_PACKAGE" "$tmpdir/rapidstream-tapa.tar.gz"
  else
    # Download the RapidStream software.
    url="${RAPIDSTREAM_UPDATE_ROOT}/rapidstream-tapa-${RAPIDSTREAM_CHANNEL}-${RAPIDSTREAM_VERSION}.tar.gz"
    if [ "$VERBOSE" = "yes" ]; then
      echo "Downloading RapidStream TAPA from: \"$url\" to \"$tmpdir/rapidstream-tapa.tar.gz\"..."
      curl_opts="-fSL"
    elif [ "$QUIET" = "no" ]; then
      echo "Downloading RapidStream TAPA..."
      curl_opts="-fsSL"
    else
      curl_opts="-fsSL"
    fi
    curl "$curl_opts" "$url" -o "$tmpdir/rapidstream-tapa.tar.gz"
  fi

  # Extract the RapidStream TAPA software.
  if [ "$VERBOSE" = "yes" ]; then
    echo "Extracting RapidStream TAPA to: \"$RAPIDSTREAM_INSTALL_DIR\"..."
    tar_opts="-xzvf"
  elif [ "$QUIET" = "no" ]; then
    echo "Extracting RapidStream TAPA..."
    tar_opts="-xzf"
  else
    tar_opts="-xzf"
  fi
  mkdir -p "$RAPIDSTREAM_INSTALL_DIR"
  tar "$tar_opts" "$tmpdir/rapidstream-tapa.tar.gz" -C "$RAPIDSTREAM_INSTALL_DIR" --overwrite

  # Clean up the temporary directory.
  if [ "$VERBOSE" = "yes" ]; then
    echo "Cleaning up the temporary directory..."
  fi
  rm -rf "$tmpdir"
}

# Create symbolic links in the system path.
create_symlinks() {
  if [ "$CREATE_SYMLINKS" = "yes" ]; then
    if [ "$QUIET" = "no" ]; then
      echo "Creating symbolic links in the system path \"/usr/local/bin\"..."
    fi

    # Create symbolic links for each executable in the installation directory.
    for bin in "$RAPIDSTREAM_INSTALL_DIR"/usr/bin/*; do
      # Skip the directories.
      if [ ! -f "$bin" ]; then
        continue
      fi

      bin_name="$(basename "$bin")"
      if [ "$VERBOSE" = "yes" ]; then
        echo "Creating symbolic link: \"/usr/local/bin/$bin_name\" -> \"$bin\"..."
      fi
      ln -sf "$bin" "/usr/local/bin/$bin_name"
    done
  fi
}

modify_profile_path_in_file() {
  profile_file="$1"

  # Check if the profile file exists.
  if [ ! -f "$profile_file" ]; then
    if [ "$VERBOSE" = "yes" ]; then
      echo "The profile file \"$profile_file\" does not exist. Skipping..."
    fi
    return
  fi

  # Check if the PATH environment variable is already modified.
  if grep -q "$RAPIDSTREAM_INSTALL_DIR" "$profile_file"; then
    if [ "$VERBOSE" = "yes" ]; then
      echo "The PATH to RapidStream TAPA is already set in \"$profile_file\". Skipping..."
    fi
    return
  fi

  # Add the PATH environment variable to the profile file.
  if [ "$QUIET" = "no" ]; then
    echo "Adding PATH to RapidStream TAPA to \"$profile_file\"..."
  fi
  echo "export PATH=\"\$PATH:$RAPIDSTREAM_INSTALL_DIR/usr/bin\"" >>"$profile_file"
}

# Modify the PATH environment variable.
modify_profile_path() {
  if [ "$MODIFY_PROFILE_PATH" = "yes" ]; then
    if [ "$VERBOSE" = "yes" ]; then
      echo "Modifying the PATH environment variable..."
    fi

    # Modify the PATH environment variable in the user's profile files.
    modify_profile_path_in_file "$HOME/.profile"
    modify_profile_path_in_file "$HOME/.bashrc"
    modify_profile_path_in_file "$HOME/.bash_profile"
    modify_profile_path_in_file "$HOME/.zshrc"
    modify_profile_path_in_file "$HOME/.zprofile"

    if [ "$QUIET" = "no" ]; then
      echo "Please restart your shell to finish the installation."
      echo "Alternatively, you can run the following command to apply the changes:"
      echo "  export PATH=\"\$PATH:$RAPIDSTREAM_INSTALL_DIR/usr/bin\""
    fi
  fi
}

main "$@"
