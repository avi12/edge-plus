using System;
using System.Reflection;

namespace FixEdge;

// What the product calls itself, written once in the csproj and read back off
// the assembly, so the exe's file properties and the panel never disagree.
internal static class Product {
  private static readonly Assembly s_assembly = typeof(Product).Assembly;

  public static string Name {
    get;
  } = Read<AssemblyProductAttribute>(attribute => attribute.Product);

  public static string Summary {
    get;
  } = Read<AssemblyDescriptionAttribute>(attribute => attribute.Description);

  public static string Author {
    get;
  } = Read<AssemblyCompanyAttribute>(attribute => attribute.Company);

  public static string Version {
    get;
  } = Read<AssemblyInformationalVersionAttribute>(attribute => attribute.InformationalVersion);

  public static Uri AuthorUri { get; } = new("https://avi12.com");

  private static string Read<TAttribute>(Func<TAttribute, string> value)
      where TAttribute : Attribute =>
      s_assembly.GetCustomAttribute<TAttribute>() is TAttribute attribute
          ? value(attribute)
          : throw new InvalidOperationException(
              $"The build did not stamp {typeof(TAttribute).Name} onto fixedge.exe");
}
