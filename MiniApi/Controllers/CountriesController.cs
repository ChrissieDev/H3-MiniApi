using Microsoft.AspNetCore.Mvc;
using MiniApi.Data;
using MiniApi.Models;
using Microsoft.EntityFrameworkCore;

namespace MiniApi.Controllers;

[ApiController]
[Route("api/countries")]

public class CountriesController : ControllerBase
{
    private readonly AppDbContext _db;

    public CountriesController(AppDbContext db)
    {
        _db = db;
    }

    [HttpPost]
    public async Task<IActionResult> Create(Country country)
    {
        if (string.IsNullOrWhiteSpace(country.Choice))
        {
            return BadRequest("choice is required");
        }

        var exists = await _db.Countries.AnyAsync(c => c.Choice.ToLower() == country.Choice.ToLower());

        if (exists)
        {
            return Conflict("country already exists");
        }

        country.Id = 0;
        country.CreatedAt = DateTime.UtcNow;

        _db.Countries.Add(country);
        await _db.SaveChangesAsync();

        return Created($"/api/countries/{country.Id}", country);
    }

    [HttpGet]
    public async Task<IActionResult> GetAll(string? region, string? search)
    {
        var query = _db.Countries.AsQueryable();

        if (!string.IsNullOrWhiteSpace(region))
        {
            query = query.Where(c => c.Region == region);
        }

        if (!string.IsNullOrWhiteSpace(search))
        {
            query = query.Where(c => EF.Functions.ILike(c.Choice, $"%{search}%"));
        }

        var countries = await query.OrderByDescending(c => c.CreatedAt).ToListAsync();
        return Ok(countries);
    }

    [HttpGet("{id}")]
    public async Task<IActionResult> GetById(int id)
    {
        var country = await _db.Countries.FindAsync(id);

        if (country == null)
        {
            return NotFound();
        }

        return Ok(country);
    }
}