using System;

namespace EdgePlus;

internal static class UpdateFeedTests {
  private static readonly Version s_running = new(0, 0, 1);
  private static readonly string s_digest = new('a', 64);

  public static void Run() {
    OffersANewerRelease();
    OffersNothingOtherwise();
  }

  private static string Manifest(string version, string url, string digest) =>
      $$"""{"version":"{{version}}","url":"{{url}}","sha256":"{{digest}}"}""";

  private static void OffersANewerRelease() {
    UpdateOffer? offer = UpdateFeed.Parse(
        Manifest("0.0.2", "https://github.com/avi12/edge-plus/releases/download/v0.0.2/EdgePlus-0.0.2.exe",
                 s_digest), s_running);
    Expect.That(offer is not null, "a newer version is offered");
    Expect.Equal(new Version(0, 0, 2), offer!.Version, "with its version");
    Expect.Equal("https://github.com/avi12/edge-plus/releases/download/v0.0.2/EdgePlus-0.0.2.exe",
                 offer.Download.ToString(), "and where to fetch it");
    Expect.Equal(new string('A', 64), offer.Sha256, "and its digest, upper case");

    Expect.That(UpdateFeed.Parse(Manifest("0.1", "https://example.com/e.exe", s_digest), s_running)
                    is not null,
                "a two-part version is a version too");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", "http://localhost:8080/e.exe", s_digest), s_running)
                    is not null,
                "a stand-in server on this machine is trusted over http");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", "http://127.0.0.1:8080/e.exe", s_digest), s_running)
                    is not null,
                "so is the loopback address");
  }

  private static void OffersNothingOtherwise() {
    const string url = "https://example.com/e.exe";
    Expect.That(UpdateFeed.Parse(Manifest("0.0.1", url, s_digest), s_running) is null,
                "the version already running is not an update");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.0", url, s_digest), s_running) is null,
                "an older one is not either");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", url, ""), s_running) is null,
                "without a digest nothing can be checked, so nothing is offered");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", url, new string('a', 63)), s_running) is null,
                "a digest of the wrong length is refused");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", url, new string('z', 64)), s_running) is null,
                "and one that is not hex");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", "http://example.com/e.exe", s_digest), s_running) is null,
                "a plain http download from elsewhere is refused");
    Expect.That(UpdateFeed.Parse(Manifest("0.0.2", "not a url", s_digest), s_running) is null,
                "a url that is not one is refused");
    Expect.That(UpdateFeed.Parse(Manifest("soon", url, s_digest), s_running) is null,
                "a version that is not one is refused");
    Expect.That(UpdateFeed.Parse("{}", s_running) is null, "an empty manifest offers nothing");
    Expect.That(UpdateFeed.Parse("[1,2]", s_running) is null, "a manifest of the wrong shape neither");
    Expect.That(UpdateFeed.Parse("not json", s_running) is null, "nor one that is not json");
    Expect.That(UpdateFeed.Parse("", s_running) is null, "nor an empty answer");
  }
}
