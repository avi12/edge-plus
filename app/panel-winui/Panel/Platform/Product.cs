using System;
using System.Reflection;

namespace EdgePlus;

// Written once in the csproj and read back off the assembly, so the exe's file
// properties and the panel never disagree.
internal static class Product {
  private static readonly Assembly s_assembly = typeof(Product).Assembly;

  public static string Name { get; } = Read<AssemblyProductAttribute>(a => a.Product);

  public static string Summary { get; } = Read<AssemblyDescriptionAttribute>(a => a.Description);

  public static string Author { get; } = Read<AssemblyCompanyAttribute>(a => a.Company);

  // AssemblyInformationalVersion is the csproj's <Version> verbatim; the other
  // two are padded to four parts.
  public static string Version {
    get;
  } = Read<AssemblyInformationalVersionAttribute>(a => a.InformationalVersion);

  public static Uri AuthorUri { get; } = new("https://avi12.com");

  public static Uri ChangelogUri { get; } = new("https://github.com/avi12/edge-plus/blob/main/CHANGELOG.md");

  private static string Read<TAttribute>(Func<TAttribute, string> value)
      where TAttribute : Attribute =>
      s_assembly.GetCustomAttribute<TAttribute>() is TAttribute attribute
          ? value(attribute)
          : throw new InvalidOperationException(
              $"The build did not stamp {typeof(TAttribute).Name} onto edgeplus.exe");
}
