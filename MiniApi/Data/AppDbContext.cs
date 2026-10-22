using Microsoft.EntityFrameworkCore;
using MiniApi.Models;

namespace MiniApi.Data;

public class AppDbContext : DbContext
{
    public AppDbContext(DbContextOptions<AppDbContext> options) : base(options)
    {

    }

    public DbSet<Country> Countries => Set<Country>();
}