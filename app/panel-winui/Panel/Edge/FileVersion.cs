using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;

namespace EdgePlus;

// A binary's four-part file version (major.minor.build.revision), compared part
// by part. Read from the version resource's fixed info, as the DLL's own
// self-check does (verify.h), so the panel and the DLL agree on what "the same
// build" means.
internal readonly record struct FileVersion(ushort Major, ushort Minor, ushort Build,
                                            ushort Revision) : IComparable<FileVersion> {
  private const int PartCount = 4;

  public override string ToString() => $"{Major}.{Minor}.{Build}.{Revision}";

  public int CompareTo(FileVersion other) {
    int byMajor = Major.CompareTo(other.Major);
    if (byMajor != 0) {
      return byMajor;
    }
    int byMinor = Minor.CompareTo(other.Minor);
    if (byMinor != 0) {
      return byMinor;
    }
    int byBuild = Build.CompareTo(other.Build);
    return byBuild != 0 ? byBuild : Revision.CompareTo(other.Revision);
  }

  public static bool operator <(FileVersion left, FileVersion right) => left.CompareTo(right) < 0;

  public static bool operator >(FileVersion left, FileVersion right) => left.CompareTo(right) > 0;

  // Exactly four dot-separated numbers that each fit a 16-bit part.
  public static FileVersion? Parse(string? text) {
    if (text is null) {
      return null;
    }
    string[] parts = text.Trim().Split('.');
    if (parts.Length != PartCount) {
      return null;
    }
    ushort[] numbers = new ushort[PartCount];
    for (int i = 0; i < PartCount; i++) {
      bool isPart = ushort.TryParse(parts[i], NumberStyles.None, CultureInfo.InvariantCulture,
                                    out numbers[i]);
      if (!isPart) {
        return null;
      }
    }
    return new FileVersion(numbers[0], numbers[1], numbers[2], numbers[3]);
  }

  // The file version of the binary at `path`, or null when it has none or
  // cannot be read.
  public static FileVersion? Of(string path) {
    try {
      FileVersionInfo info = FileVersionInfo.GetVersionInfo(path);
      var version = new FileVersion((ushort)info.FileMajorPart, (ushort)info.FileMinorPart,
                                    (ushort)info.FileBuildPart, (ushort)info.FilePrivatePart);
      // A binary with no version resource reads as all zeros.
      bool hasVersionResource = version != default;
      return hasVersionResource ? version : null;
    } catch (FileNotFoundException) {
      return null;
    } catch (ArgumentException) {
      return null;
    }
  }
}
