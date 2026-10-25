using Microsoft.AspNetCore.Components;
using Microsoft.AspNetCore.Components.Web;
using MiniApi.Models;

namespace MiniApi.Components.Pages
{
    public partial class Home : ComponentBase
    {
        [Inject]
        private IHttpClientFactory HttpClientFactory { get; set; } = default!;

        protected IReadOnlyList<Country> Countries { get; private set; } = [];
        protected IReadOnlyList<string> Regions { get; } = ["Africa", "Americas", "Asia", "Europe", "Oceania", "Polar"];
        protected string SearchText { get; set; } = string.Empty;
        protected string SelectedRegion { get; set; } = string.Empty;
        protected string HelloMessage { get; private set; } = "Connecting...";
        protected string? ErrorMessage { get; private set; }
        protected Country? SelectedCountry { get; private set; }
        protected bool IsDetailsOpen { get; set; }
        protected bool IsLoading { get; private set; }

        protected static class ApiSources
        {
            public const string Hello = "http://192.168.1.191:5144/api/hello";
            public const string Countries = "http://192.168.1.191:5144/api/countries";
            public const string CountryById = "http://192.168.1.191:5144/api/countries/{0}";
            public const string OpenApi = "http://192.168.1.191:5144/openapi/v1.json";
        }

        protected override async Task OnInitializedAsync()
        {
            await LoadDashboardAsync();
        }

        protected async Task LoadDashboardAsync()
        {
            IsLoading = true;
            ErrorMessage = null;
            try
            {
                var client = HttpClientFactory.CreateClient("MiniApi");
                HelloMessage = await client.GetStringAsync(ApiSources.Hello);
                await ApplyFiltersAsync();
            }
            catch (HttpRequestException)
            {
                HelloMessage = "Offline";
                ErrorMessage = "The MiniApi could not be reached. Check that the API is running and try again.";
            }
            finally
            {
                IsLoading = false;
            }
        }

        protected async Task ApplyFiltersAsync()
        {
            IsLoading = true;
            ErrorMessage = null;
            try
            {
                var query = new List<string>();
                if (!string.IsNullOrWhiteSpace(SelectedRegion))
                {
                    query.Add($"region={Uri.EscapeDataString(SelectedRegion)}");
                }
                if (!string.IsNullOrWhiteSpace(SearchText))
                {
                    query.Add($"search={Uri.EscapeDataString(SearchText.Trim())}");
                }

                var endpoint = ApiSources.Countries;
                if (query.Count > 0)
                {
                    endpoint += "?" + string.Join("&", query);
                }

                var client = HttpClientFactory.CreateClient("MiniApi");
                Countries = await client.GetFromJsonAsync<List<Country>>(endpoint) ?? [];
            }
            catch (HttpRequestException)
            {
                ErrorMessage = "Country data could not be loaded. Please try again.";
            }
            finally
            {
                IsLoading = false;
            }
        }

        protected async Task ShowDetailsAsync(int id)
        {
            try
            {
                var client = HttpClientFactory.CreateClient("MiniApi");
                SelectedCountry = await client.GetFromJsonAsync<Country>(string.Format(ApiSources.CountryById, id));
                IsDetailsOpen = SelectedCountry is not null;
            }
            catch (HttpRequestException)
            {
                ErrorMessage = "The country details could not be loaded.";
            }
        }

        protected async Task OnRegionChanged(ChangeEventArgs args)
        {
            SelectedRegion = args.Value?.ToString() ?? string.Empty;
            await ApplyFiltersAsync();
        }

        protected async Task OnSearchKeyUp(KeyboardEventArgs args)
        {
            if (args.Key == "Enter")
            {
                await ApplyFiltersAsync();
            }
        }

        protected void CloseDetails()
        {
            IsDetailsOpen = false;
        }
    }
}
