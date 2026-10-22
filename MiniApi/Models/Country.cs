namespace MiniApi.Models;

public class Country
{
    // from backend
    public int Id { get; set; }
    public DateTime CreatedAt { get; set; }

    // from Arduino
    public string ApiName { get; set; } = "";
    public string Choice { get; set; } = "";
    public string? Cca3 { get; set; }
    public string? Capital { get; set; }
    public string? Region { get; set; }
    public string? Subregion { get; set; }
    public long? Population { get; set; }
    public double? Area { get; set; }
    public double? Latitude { get; set; }
    public double? Longitude { get; set; }
    public string? FlagUrl { get; set; }
}