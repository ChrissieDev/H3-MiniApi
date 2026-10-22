using Microsoft.AspNetCore.Mvc;

namespace MiniApi.Controllers;

[ApiController]
[Route("api/hello")]

public class HelloController : ControllerBase
{
    [HttpGet]
    public string Get()
    {
        return "Hi from MiniApi";
    }
}
