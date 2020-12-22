//
// Scanner web interface functions for the Scanner Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local functions...
//

static void	job_cb(pappl_job_t *job, pappl_client_t *client);
static char	*localize_keyword(const char *attrname, const char *keyword, char *buffer, size_t bufsize);
static char	*localize_media(pappl_media_col_t *media, bool include_source, char *buffer, size_t bufsize);
static void	media_chooser(pappl_client_t *client, pappl_pr_driver_data_t *driver_data, const char *title, const char *name, pappl_media_col_t *media);
static char	*time_string(time_t tv, char *buffer, size_t bufsize);
static void	job_pager(pappl_client_t *client, pappl_printer_t *Scanner, int job_index, int limit);


//
// '_papplScannerWebCancelAllJobs()' - Cancel all Scanner jobs.
//

void
_papplScannerWebCancelAllJobs(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      char	path[1024];		// Resource path

      papplScannerCancelAllJobs(Scanner);
      snprintf(path, sizeof(path), "%s/jobs", Scanner->uriname);
      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, path);
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Cancel All Jobs", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client, "           <input type=\"submit\" value=\"Confirm Cancel All\"></form>");

  if (papplScannerGetNumberOfActiveJobs(Scanner) > 0)
  {
    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages Completed</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateActiveJobs(Scanner, (pappl_job_cb_t)job_cb, client, 1, 0);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");
  }
  else
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");

  papplClientHTMLFooter(client);
}


//
// '_papplScannerWebCancelJob()' - Cancel a job. 
//

void
_papplScannerWebCancelJob(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  int		job_id = 0;             // Job ID to cancel
  pappl_job_t	*job;			// Job to cancel
  const char	*status = NULL;		// Status message, if any
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*value;			// Value of form variable


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_GET)
  {

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid GET data.";
    }
    else if ((value = cupsGetOption("job-id", num_form, form)) != NULL)
      job_id = atoi(value);

    cupsFreeOptions(num_form, form);
  }
  else if (client->operation == HTTP_STATE_POST)
  {
    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if ((value = cupsGetOption("job-id", num_form, form)) != NULL)
    {
      // Get the job to cancel
      if ((job = papplScannerFindJob(Scanner, atoi(value))) != NULL)
      {
        char path[1024];		// Resource path

        papplJobCancel(job);
        snprintf(path, sizeof(path), "%s/jobs", Scanner->uriname);
        papplClientRespondRedirect(client, HTTP_STATUS_FOUND, path);
        cupsFreeOptions(num_form, form);
        return;
      }
      else
      {
        status = "Invalid Job ID.";
      }
    }
    else
    {
      status = "Invalid form submission.";
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Cancel Job", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  if (job_id)
  {
    papplClientHTMLStartForm(client, client->uri, false);
    papplClientHTMLPrintf(client, "           <input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"Confirm Cancel Job\"></form>\n", job_id);
  }

  papplClientHTMLFooter(client);
}


//
// '_papplScannerWebConfig()' - Show the Scanner configuration web page.
//

void
_papplScannerWebConfig(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any
  char		dns_sd_name[64],	// DNS-SD name
		location[128],		// Location
		geo_location[128],	// Geo-location latitude
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      _papplScannerWebConfigFinalize(Scanner, num_form, form);

      if (Scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
        _papplScannerWebDelete(client, Scanner);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Configuration", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  _papplClientHTMLInfo(client, true, papplScannerGetDNSSDName(Scanner, dns_sd_name, sizeof(dns_sd_name)), papplScannerGetLocation(Scanner, location, sizeof(location)), papplScannerGetGeoLocation(Scanner, geo_location, sizeof(geo_location)), papplScannerGetOrganization(Scanner, organization, sizeof(organization)), papplScannerGetOrganizationalUnit(Scanner, org_unit, sizeof(org_unit)), papplScannerGetContact(Scanner, &contact));

  papplClientHTMLScannerFooter(client);
}


//
// '_papplScannerWebConfigFinalize()' - Save the changes to the Scanner configuration.
//

void
_papplScannerWebConfigFinalize(
    pappl_printer_t *Scanner,		// I - Scanner
    int             num_form,		// I - Number of form variables
    cups_option_t   *form)		// I - Form variables
{
  const char	*value,			// Form value
		*geo_lat,		// Geo-location latitude
		*geo_lon,		// Geo-location longitude
		*contact_name,		// Contact name
		*contact_email,		// Contact email
		*contact_tel;		// Contact telephone number


  if ((value = cupsGetOption("dns_sd_name", num_form, form)) != NULL)
    papplScannerSetDNSSDName(Scanner, *value ? value : NULL);

  if ((value = cupsGetOption("location", num_form, form)) != NULL)
    papplScannerSetLocation(Scanner, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", atof(geo_lat), atof(geo_lon));
      papplScannerSetGeoLocation(Scanner, uri);
    }
    else
      papplScannerSetGeoLocation(Scanner, NULL);
  }

  if ((value = cupsGetOption("organization", num_form, form)) != NULL)
    papplScannerSetOrganization(Scanner, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", num_form, form)) != NULL)
    papplScannerSetOrganizationalUnit(Scanner, *value ? value : NULL);

  contact_name  = cupsGetOption("contact_name", num_form, form);
  contact_email = cupsGetOption("contact_email", num_form, form);
  contact_tel   = cupsGetOption("contact_telephone", num_form, form);
  if (contact_name || contact_email || contact_tel)
  {
    pappl_contact_t	contact;	// Contact info

    memset(&contact, 0, sizeof(contact));

    if (contact_name)
      strlcpy(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      strlcpy(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      strlcpy(contact.telephone, contact_tel, sizeof(contact.telephone));

    papplScannerSetContact(Scanner, &contact);
  }
}


//
// '_papplScannerWebDefaults()' - Show the Scanner defaults web page.
//

void
_papplScannerWebDefaults(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  int			i, j;		// Looping vars
  pappl_pr_driver_data_t data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    "Portrait",
    "Landscape",
    "Reverse Landscape",
    "Reverse Portrait",
    "Auto"
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  papplScannerGetDriverData(Scanner, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    int			num_vendor = 0;	// Number of vendor options
    cups_option_t	*vendor = NULL;	// Vendor options

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      const char	*value;		// Value of form variable

      if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
        data.orient_default = (ipp_orient_t)atoi(value);

      if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_bin; i ++)
        {
          if (!strcmp(data.bin[i], value))
          {
            data.bin_default = i;
            break;
          }
	}
      }

      if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
        data.color_default = _papplColorModeValue(value);

      if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
        data.content_default = _papplContentValue(value);

      if ((value = cupsGetOption("print-darkness", num_form, form)) != NULL)
        data.darkness_configured = atoi(value);

      if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
        data.quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);

      if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
        data.scaling_default = _papplScalingValue(value);

      if ((value = cupsGetOption("print-speed", num_form, form)) != NULL)
        data.speed_default = atoi(value) * 2540;

      if ((value = cupsGetOption("sides", num_form, form)) != NULL)
        data.sides_default = _papplSidesValue(value);

      if ((value = cupsGetOption("Scanner-resolution", num_form, form)) != NULL)
      {
        if (sscanf(value, "%dx%ddpi", &data.x_default, &data.y_default) == 1)
          data.y_default = data.x_default;
      }

      if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_source; i ++)
	{
	  if (!strcmp(value, data.source[i]))
	  {
	    data.media_default = data.media_ready[i];
	    break;
	  }
	}
      }

      for (i = 0; i < data.num_vendor; i ++)
      {
        char	supattr[128];		// xxx-supported

        snprintf(supattr, sizeof(supattr), "%s-supported", data.vendor[i]);

        if ((value = cupsGetOption(data.vendor[i], num_form, form)) != NULL)
	  num_vendor = cupsAddOption(data.vendor[i], value, num_vendor, &vendor);
	else if (ippFindAttribute(Scanner->driver_attrs, supattr, IPP_TAG_BOOLEAN))
	  num_vendor = cupsAddOption(data.vendor[i], "false", num_vendor, &vendor);
      }

      papplScannerSetDriverDefaults(Scanner, &data, num_vendor, vendor);

      cupsFreeOptions(num_vendor, vendor);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Printing Defaults", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  // media-col-default
  papplClientHTMLPuts(client, "              <tr><th>Media:</th><td><select name=\"media-source\">");
  for (i = 0; i < data.num_source; i ++)
  {
    // See if any two sources have the same size...
    for (j = i + 1; j < data.num_source; j ++)
    {
      if (data.media_ready[i].size_width > 0 && data.media_ready[i].size_width == data.media_ready[j].size_width && data.media_ready[i].size_length == data.media_ready[j].size_length)
      {
        show_source = true;
        break;
      }
    }
  }

  for (i = 0; i < data.num_source; i ++)
  {
    keyword = data.source[i];

    if (strcmp(keyword, "manual"))
    {
      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, data.media_default.source) ? " selected" : "", localize_media(data.media_ready + i, show_source, text, sizeof(text)));
    }
  }
  papplClientHTMLPrintf(client, "</select> <a class=\"btn\" href=\"%s/media\">Configure Media</a></td></tr>\n", Scanner->uriname);

  // orientation-requested-default
  papplClientHTMLPuts(client, "              <tr><th>Orientation:</th><td>");
  for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
  {
    papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, data.orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // print-color-mode-default
  papplClientHTMLPuts(client, "              <tr><th>Print Mode:</th><td>");
  if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
  {
    papplClientHTMLPuts(client, "B&amp;W");
  }
  else
  {
    for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
    {
      if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
      {
	keyword = _papplColorModeString((pappl_color_mode_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-color-mode\" value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == data.color_default ? " checked" : "", localize_keyword("print-color-mode", keyword, text, sizeof(text)));
      }
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPuts(client, "              <tr><th>2-Sided Printing:</th><td>");
    for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
    {
      if (data.sides_supported & (pappl_sides_t)i)
      {
	keyword = _papplSidesString((pappl_sides_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\" value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == data.sides_default ? " checked" : "", localize_keyword("sides", keyword, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (data.num_bin > 0)
  {
    papplClientHTMLPuts(client, "              <tr><th>Output Tray:</th><td>");
    if (data.num_bin > 1)
    {
      papplClientHTMLPuts(client, "<select name=\"output-bin\">");
      for (i = 0; i < data.num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", data.bin[i], i == data.bin_default ? " selected" : "", localize_keyword("output-bin", data.bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword("output-bin", data.bin[data.bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPuts(client, "              <tr><th>Print Quality:</th><td>");
  for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
  {
    keyword = ippEnumString("print-quality", i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-quality\" value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == data.quality_default ? " checked" : "", localize_keyword("print-quality", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-darkness-configured
  if (data.darkness_supported)
  {
    papplClientHTMLPuts(client, "              <tr><th>Print Darkness:</th><td><select name=\"print-darkness\">");
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, percent == data.darkness_configured ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPuts(client, "              <tr><th>Print Speed:</th><td><select name=\"print-speed\"><option value=\"0\">Auto</option>");
    for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
    {
      if (i > 0)
	papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d %s/sec</option>", i / 2540, i == data.speed_default ? " selected" : "", i / 2540, i >= (2 * 2540) ? "inches" : "inch");
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-content-optimize-default
  papplClientHTMLPuts(client, "              <tr><th>Optimize For:</th><td><select name=\"print-content-optimize\">");
  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString((pappl_content_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_content_t)i == data.content_default ? " selected" : "", localize_keyword("print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPuts(client, "              <tr><th>Scaling:</th><td><select name=\"print-scaling\">");
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == data.scaling_default ? " selected" : "", localize_keyword("print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // Scanner-resolution-default
  papplClientHTMLPuts(client, "              <tr><th>Resolution:</th><td>");

  if (data.num_resolution == 1)
  {
    if (data.x_resolution[0] != data.y_resolution[0])
      papplClientHTMLPrintf(client, "%dx%ddpi", data.x_resolution[0], data.y_resolution[0]);
    else
      papplClientHTMLPrintf(client, "%ddpi", data.x_resolution[0]);
  }
  else
  {
    papplClientHTMLPuts(client, "<select name=\"Scanner-resolution\">");
    for (i = 0; i < data.num_resolution; i ++)
    {
      if (data.x_resolution[i] != data.y_resolution[i])
	snprintf(text, sizeof(text), "%dx%ddpi", data.x_resolution[i], data.y_resolution[i]);
      else
	snprintf(text, sizeof(text), "%ddpi", data.x_resolution[i]);

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (data.x_default == data.x_resolution[i] && data.y_default == data.y_resolution[i]) ? " selected" : "", text);
    }
    papplClientHTMLPuts(client, "</select>");
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // Vendor options
  pthread_rwlock_rdlock(&Scanner->rwlock);

  for (i = 0; i < data.num_vendor; i ++)
  {
    char	defname[128],		// xxx-default name
		defvalue[1024],		// xxx-default value
		supname[128];		// xxx-supported name
    ipp_attribute_t *attr;		// Attribute
    int		count;			// Number of values

    snprintf(defname, sizeof(defname), "%s-default", data.vendor[i]);
    snprintf(supname, sizeof(defname), "%s-supported", data.vendor[i]);

    if ((attr = ippFindAttribute(Scanner->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
      ippAttributeString(attr, defvalue, sizeof(defvalue));
    else
      defvalue[0] = '\0';

    if ((attr = ippFindAttribute(Scanner->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {
      count = ippGetCount(attr);

      papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", data.vendor[i]);

      switch (ippGetValueTag(attr))
      {
        case IPP_TAG_BOOLEAN :
            papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            break;

        case IPP_TAG_INTEGER :
            papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              int val = ippGetInteger(attr, j);

	      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d</option>", val, val == atoi(defvalue) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

        case IPP_TAG_RANGE :
            {
              int upper, lower = ippGetRange(attr, 0, &upper);
					// Range

	      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);
	    }
            break;

        case IPP_TAG_KEYWORD :
            papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              const char *val = ippGetString(attr, j, NULL);

	      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, !strcmp(val, defvalue) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

	default :
	    papplClientHTMLPuts(client, "Unsupported value syntax.");
	    break;
      }

      papplClientHTMLPuts(client, "</td></tr>\n");
    }
    else
    {
      // No xxx-supported, so this is just a text field...
      papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><input name=\"%s\" value=\"%s\"></td></tr>\n", data.vendor[i], data.vendor[i], defvalue);
    }
  }

  pthread_rwlock_unlock(&Scanner->rwlock);

  papplClientHTMLPuts(client,
                      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
                      "            </tbody>\n"
                      "          </table>"
                      "        </form>\n");

  papplClientHTMLScannerFooter(client);
}


//
// '_papplScannerWebDelete()' - Show the Scanner delete confirmation web page.
//

void
_papplScannerWebDelete(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if (Scanner->processing_job)
    {
      // Scanner is processing a job...
      status = "Scanner is currently active.";
    }
    else
    {
      if (!Scanner->is_deleted)
      {
        papplScannerDelete(Scanner);
        Scanner = NULL;
      }

      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/");
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Delete Scanner", 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPuts(client,"          <input type=\"submit\" value=\"Confirm Delete Scanner\"></form>");

  papplClientHTMLFooter(client);
}


//
// '_papplScannerWebHome()' - Show the Scanner home page.
//

void
_papplScannerWebHome(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  const char	*status = NULL;		// Status text, if any
  ipp_pstate_t	Scanner_state;		// Scanner state
  char		edit_path[1024];	// Edit configuration URL
  const int	limit = 20;		// Jobs per page
  int		job_index = 1;		// Job index


  // Handle POSTs to print a test page...
  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*action;	// Form action

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if ((action = cupsGetOption("action", num_form, form)) == NULL)
    {
      status = "Missing action.";
    }
    else if (!strcmp(action, "identify-Scanner"))
    {
      if (Scanner->driver_data.identify_supported && Scanner->driver_data.identify_cb)
      {
        (Scanner->driver_data.identify_cb)(Scanner, Scanner->driver_data.identify_supported, "Hello.");

        status = "Scanner identified.";
      }
      else
      {
        status = "Unable to identify Scanner.";
      }
    }
    else if (!strcmp(action, "print-test-page"))
    {
      pappl_job_t	*job;		// New job
      const char	*filename;	// Test Page filename
      char		buffer[1024],	// File Buffer
			*username;	// Username

      // Get the testfile to scan, if any...
      if (Scanner->driver_data.testpage_cb)
        filename = (Scanner->driver_data.testpage_cb)(Scanner, buffer, sizeof(buffer));
      else
	filename = NULL;

      if (filename)
      {
        // Have a file to scan, so create a job and scan it...
        if (client->username[0])
          username = client->username;
        else
          username = "guest";

        if (access(filename, R_OK))
        {
          status = "Unable to access test scan file.";
        }
        else if ((job = _papplJobCreate(Scanner, 0, username, NULL, "Test Page", NULL)) == NULL)
        {
          status = "Unable to create test scan job.";
        }
        else
        {
          // Submit the job for processing...
          _papplJobSubmitFile(job, filename);

          status = "Test page scan.";
        }
      }
      else
        status = "Test page scan.";
    }
    else
      status = "Unknown action.";

    cupsFreeOptions(num_form, form);
  }

  // Show status...
  Scanner_state = papplScannerGetState(Scanner);

  papplClientHTMLScannerHeader(client, Scanner, NULL, Scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");

  _papplScannerWebIteratorCallback(Scanner, client);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  snprintf(edit_path, sizeof(edit_path), "%s/config", Scanner->uriname);
  papplClientHTMLPrintf(client, "          <h1 class=\"title\">Configuration <a class=\"btn\" href=\"https://%s:%d%s\">Change</a></h1>\n", client->host_field, client->host_port, edit_path);

  _papplClientHTMLInfo(client, false, Scanner->dns_sd_name, Scanner->location, Scanner->geo_location, Scanner->organization, Scanner->org_unit, &Scanner->contact);

  if (!(Scanner->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    _papplSystemWebSettings(client);

  papplClientHTMLPrintf(client,
			"        </div>\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\"><a href=\"%s/jobs\">Jobs</a>", Scanner->uriname);

  if (papplScannerGetNumberOfJobs(Scanner) > 0)
  {
    if (cupsArrayCount(Scanner->active_jobs) > 0)
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s/cancelall\">Cancel All Jobs</a></h1>\n", client->host_field, client->host_port, Scanner->uriname);
    else
      papplClientHTMLPuts(client, "</h1>\n");

    job_pager(client, Scanner, job_index, limit);

    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateAllJobs(Scanner, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, Scanner, job_index, limit);
  }
  else
    papplClientHTMLPuts(client,
			"</h1>\n"
                        "        <p>No jobs in history.</p>\n");

  papplClientHTMLScannerFooter(client);
}


//
// '_papplScannerWebIteratorCallback()' - Show the Scanner status. -fine
//

void
_papplScannerWebIteratorCallback(
    pappl_printer_t *Scanner,		// I - Scanner
    pappl_client_t  *client)		// I - Client
{
  int			i;		// Looping var
  pappl_preason_t	reason,		// Current reason
			Scanner_reasons;// Scanner state reasons
  ipp_pstate_t		Scanner_state;	// Scanner state
  int			Scanner_jobs;	// Number of queued jobs
  char			uri[256];	// Form URI
  static const char * const states[] =	// State strings
  {
    "Idle",
    "Scanning",
    "Stopped"
  };
  static const char * const reasons[] =	// Reason strings
  {
    "Other",
    "Cover Open",
    "No Media in Scanner"
  };


  Scanner_jobs    = papplScannerGetNumberOfActiveJobs(Scanner);
  Scanner_state   = papplScannerGetState(Scanner);
  Scanner_reasons = papplScannerGetReasons(Scanner);

  snprintf(uri, sizeof(uri), "%s/", Scanner->uriname);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a> <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete</a></h2>\n", Scanner->uriname, Scanner->name, client->host_field, client->host_port, Scanner->uriname);
  else
    papplClientHTMLPuts(client, "          <h1 class=\"title\">Status</h1>\n");

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %d %s", ippEnumString("Scanner-state", (int)Scanner_state), Scanner->uriname, states[Scanner_state - IPP_PSTATE_IDLE], Scanner_jobs, Scanner_jobs == 1 ? "job" : "jobs");
  for (i = 0, reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; i ++, reason *= 2)
  {
    if (Scanner_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", reasons[i]);
  }

  if (strcmp(Scanner->name, Scanner->driver_data.make_and_model))
    papplClientHTMLPrintf(client, ".<br>%s</p>\n", Scanner->driver_data.make_and_model);
  else
    papplClientHTMLPuts(client, ".</p>\n");

  papplClientHTMLPrintf(client,
                        "          <div class=\"btn\"><a class=\"btn\" href=\"%s/media\">Media</a> <a class=\"btn\" href=\"%s/printing\">Scanning Defaults</a>", Scanner->uriname, Scanner->uriname);
  if (Scanner->driver_data.has_supplies)
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/supplies\">Supplies</a>", Scanner->uriname);

  if (Scanner->driver_data.identify_supported)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"identify-Scanner\"><input type=\"submit\" value=\"Identify Scanner\"></form>");
  }

  if (Scanner->driver_data.testpage_cb)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"Scan-test-page\"><input type=\"submit\" value=\"Print Test Page\"></form>");
  }

  if (strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete Scanner</a>", client->host_field, client->host_port, Scanner->uriname);

  papplClientHTMLPuts(client, "<br clear=\"all\"></div>\n");
}

//
// '_papplScannerWebIteratorCallback()' - Show the scanner status. - fine
//

void
_papplScannerWebIteratorCallback(
    pappl_printer_t *Scanner,		// I - Scanner
    pappl_client_t  *client)		// I - Client
{
  int			i;		// Looping var
  pappl_preason_t	reason,		// Current reason
			Scanner_reasons;// Scanner state reasons
  ipp_pstate_t		Scanner_state;	// Scanner state
  int			Scanner_jobs;	// Number of queued jobs
  char			uri[256];	// Form URI
  static const char * const states[] =	// State strings
  {
    "Idle",
    "Printing",
    "Stopped"
  };
  static const char * const reasons[] =	// Reason strings
  {
    "Other",
    "Cover Open",
    "Too Many Jobs",
    "No Media in Scanner"
  };


  Scanner_jobs    = papplScannerGetNumberOfActiveJobs(Scanner);
  Scanner_state   = papplScannerGetState(Scanner);
  Scanner_reasons = papplScannerGetReasons(Scanner);

  snprintf(uri, sizeof(uri), "%s/", Scanner->uriname);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a> <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete</a></h2>\n", Scanner->uriname, Scanner->name, client->host_field, client->host_port, Scanner->uriname);
  else
    papplClientHTMLPuts(client, "          <h1 class=\"title\">Status</h1>\n");

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %d %s", ippEnumString("Scanner-state", (int)Scanner_state), Scanner->uriname, states[Scanner_state - IPP_PSTATE_IDLE], Scanner_jobs, Scanner_jobs == 1 ? "job" : "jobs");
  for (i = 0, reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; i ++, reason *= 2)
  {
    if (Scanner_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", reasons[i]);
  }

  if (strcmp(Scanner->name, Scanner->driver_data.make_and_model))
    papplClientHTMLPrintf(client, ".<br>%s</p>\n", Scanner->driver_data.make_and_model);
  else
    papplClientHTMLPuts(client, ".</p>\n");

  papplClientHTMLPrintf(client,
                        "          <div class=\"btn\"><a class=\"btn\" href=\"%s/media\">Media</a> <a class=\"btn\" href=\"%s/scanning\">Scanning Defaults</a>", Scanner->uriname, Scanner->uriname);
  if (Scanner->driver_data.has_supplies)
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/supplies\">Supplies</a>", Scanner->uriname);

  if (Scanner->driver_data.identify_supported)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"identify-Scanner\"><input type=\"submit\" value=\"Identify Scanner\"></form>");
  }

  if (Scanner->driver_data.testpage_cb)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"scan-test-page\"><input type=\"submit\" value=\"Scan Test Page\"></form>");
  }

  if (strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"https://%s:%d%s/delete\">Delete Scanner</a>", client->host_field, client->host_port, Scanner->uriname);

  papplClientHTMLPuts(client, "<br clear=\"all\"></div>\n");
}


//
// '_papplScannerWebJobs()' - Show the Scanner jobs web page.
//

void
_papplScannerWebJobs(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  ipp_pstate_t	Scanner_state;		// Scanner state
  int		job_index = 1,		// Job index
		limit = 20;		// Jobs per page


  if (!papplClientHTMLAuthorize(client))
    return;

  Scanner_state = papplScannerGetState(Scanner);

  if (client->operation == HTTP_STATE_GET)
  {
    cups_option_t	*form = NULL;	// Form variables
    int			num_form = papplClientGetForm(client, &form);
					// Number of form variables
    const char		*value = NULL;	// Value of form variable

    if ((value = cupsGetOption("job-index", num_form, form)) != NULL)
      job_index = atoi(value);

    cupsFreeOptions(num_form, form);
  }

  if (cupsArrayCount(Scanner->active_jobs) > 0)
  {
    char	url[1024];		// URL for Cancel All Jobs

    httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), "https", NULL, client->host_field, client->host_port, "%s/cancelall", Scanner->uriname);

    papplClientHTMLScannerHeader(client, Scanner, "Jobs", Scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, "Cancel All Jobs", url);
  }
  else
  {
    papplClientHTMLScannerHeader(client, Scanner, "Jobs", Scanner_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);
  }

  if (papplScannerGetNumberOfJobs(Scanner) > 0)
  {
    job_pager(client, Scanner, job_index, limit);

    papplClientHTMLPuts(client,
			"          <table class=\"list\" summary=\"Jobs\">\n"
			"            <thead>\n"
			"              <tr><th>Job #</th><th>Name</th><th>Owner</th><th>Pages Completed</th><th>Status</th><th></th></tr>\n"
			"            </thead>\n"
			"            <tbody>\n");

    papplScannerIterateAllJobs(Scanner, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, Scanner, job_index, limit);
  }
  else
    papplClientHTMLPuts(client, "        <p>No jobs in history.</p>\n");

  papplClientHTMLScannerFooter(client);
}


//
// '_papplScannerWebMedia()' - Show the Scanner media web page.
//

void
_papplScannerWebMedia(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *Scanner)		// I - Scanner
{
  int			i;		// Looping var
  pappl_pr_driver_data_t data;		// Driver data
  char			name[32],	// Prefix (readyN)
			text[256];	// Localized media-souce name
  const char		*status = NULL;	// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  papplScannerGetDriverData(Scanner, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else
    {
      pwg_media_t	*pwg = NULL;	// PWG media info
      pappl_media_col_t	*ready;		// Current ready media
      const char	*value,		// Value of form variable
			*custom_width,	// Custom media width
			*custom_length;	// Custom media length

      memset(data.media_ready, 0, sizeof(data.media_ready));
      for (i = 0, ready = data.media_ready; i < data.num_source; i ++, ready ++)
      {
        // size
        snprintf(name, sizeof(name), "ready%d-size", i);
        if ((value = cupsGetOption(name, num_form, form)) == NULL)
          continue;

        if (!strcmp(value, "custom"))
        {
          // Custom size...
          snprintf(name, sizeof(name), "ready%d-custom-width", i);
          custom_width = cupsGetOption(name, num_form, form);
          snprintf(name, sizeof(name), "ready%d-custom-length", i);
          custom_length = cupsGetOption(name, num_form, form);

          if (custom_width && custom_length)
            pwg = pwgMediaForSize((int)(2540.0 * atof(custom_width)), (int)(2540.0 * atof(custom_length)));
        }
        else
        {
          // Standard size...
          pwg = pwgMediaForPWG(value);
        }

        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s='%s',%d,%d", name, pwg ? pwg->pwg : "unknown", pwg ? pwg->width : 0, pwg ? pwg->length : 0);

        if (pwg)
        {
          strlcpy(ready->size_name, pwg->pwg, sizeof(ready->size_name));
          ready->size_width  = pwg->width;
          ready->size_length = pwg->length;
        }

        // source
        strlcpy(ready->source, data.source[i], sizeof(ready->source));

        // margins
        snprintf(name, sizeof(name), "ready%d-borderless", i);
        if (cupsGetOption(name, num_form, form))
	{
	  ready->bottom_margin = ready->top_margin = 0;
	  ready->left_margin = ready->right_margin = 0;
	}
	else
	{
	  ready->bottom_margin = ready->top_margin = data.bottom_top;
	  ready->left_margin = ready->right_margin = data.left_right;
	}

        // top-offset
        snprintf(name, sizeof(name), "ready%d-top-offset", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->top_offset = (int)(100.0 * atof(value));

        // tracking
        snprintf(name, sizeof(name), "ready%d-tracking", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->tracking = _papplMediaTrackingValue(value);

        // type
        snprintf(name, sizeof(name), "ready%d-type", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          strlcpy(ready->type, value, sizeof(ready->type));
      }

      papplScannerSetReadyMedia(Scanner, data.num_source, data.media_ready);

      status = "Changes saved.";
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLScannerHeader(client, Scanner, "Media", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  for (i = 0; i < data.num_source; i ++)
  {
    if (!strcmp(data.source[i], "manual"))
      continue;

    snprintf(name, sizeof(name), "ready%d", i);
    media_chooser(client, &data, localize_keyword("media-source", data.source[i], text, sizeof(text)), name, data.media_ready + i);
  }

  papplClientHTMLPuts(client,
                      "              <tr><th></th><td><input type=\"submit\" value=\"Save Changes\"></td></tr>\n"
                      "            </tbody>\n"
                      "          </table>"
		      "        </form>\n"
		      "        <script>function show_hide_custom(name) {\n"
		      "  let selelem = document.forms['form'][name + '-size'];\n"
		      "  let divelem = document.getElementById(name + '-custom');\n"
		      "  if (selelem.selectedIndex == 0)\n"
		      "    divelem.style = 'display: inline-block;';\n"
		      "  else\n"
		      "    divelem.style = 'display: none;';\n"
		      "}</script>\n");

  papplClientHTMLScannerFooter(client);
}


//
// 'job_cb()' - Job iterator callback.
//

static void
job_cb(pappl_job_t    *job,		// I - Job
       pappl_client_t *client)		// I - Client
{
  bool	show_cancel = false;		// Show the "cancel" button?
  char	when[256],			// When job queued/started/finished
      	hhmmss[64];			// Time HH:MM:SS


  switch (papplJobGetState(job))
  {
    case IPP_JSTATE_PENDING :
    case IPP_JSTATE_HELD :
	show_cancel = true;
	snprintf(when, sizeof(when), "Queued at %s", time_string(papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	if (papplJobIsCanceled(job))
	{
	  strlcpy(when, "Canceling", sizeof(when));
	}
	else
	{
	  show_cancel = true;
	  snprintf(when, sizeof(when), "Started at %s", time_string(papplJobGetTimeProcessed(job), hhmmss, sizeof(hhmmss)));
	}
	break;

    case IPP_JSTATE_ABORTED :
	snprintf(when, sizeof(when), "Aborted at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_CANCELED :
	snprintf(when, sizeof(when), "Canceled at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_COMPLETED :
	snprintf(when, sizeof(when), "Completed at %s", time_string(papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;
  }

  papplClientHTMLPrintf(client, "              <tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td>", papplJobGetID(job), papplJobGetName(job), papplJobGetUsername(job), papplJobGetImpressionsCompleted(job), when);

  if (show_cancel)
    papplClientHTMLPrintf(client, "          <td><a class=\"btn\" href=\"%s/cancel?job-id=%d\">Cancel Job</a></td></tr>\n", job->Scanner->uriname, papplJobGetID(job));
  else
    papplClientHTMLPuts(client, "<td></td></tr>\n");
}


//
// 'job_pager()' - Show the job paging links.
//

static void
job_pager(pappl_client_t  *client,	// I - Client
	  pappl_printer_t *Scanner,	// I - Scanner
	  int             job_index,	// I - First job shown (1-based)
	  int             limit)	// I - Maximum jobs shown
{
  int	num_jobs = 0,			// Number of jobs
	num_pages = 0,			// Number of pages
	i,				// Looping var
	page = 0;			// Current page
  char	path[1024];			// resource path


  if ((num_jobs = papplScannerGetNumberOfJobs(Scanner)) <= limit)
    return;

  num_pages = (num_jobs + limit - 1) / limit;
  page      = (job_index - 1) / limit;

  snprintf(path, sizeof(path), "%s/jobs", Scanner->uriname);

  papplClientHTMLPuts(client, "          <div class=\"pager\">");

  if (page > 0)
    papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"%s?job-index=%d\">&laquo;</a>", path, (page - 1) * limit + 1);

  for (i = 0; i < num_pages; i ++)
  {
    if (i == page)
      papplClientHTMLPrintf(client, " %d", i + 1);
    else
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">%d</a>", path, i * limit + 1, i + 1);
  }

  if (page < (num_pages - 1))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">&raquo;</a>", path, (page + 1) * limit + 1);

  papplClientHTMLPuts(client, "</div>\n");
}


//
// 'localize_keyword()' - Localize a media keyword...
//

static char *				// O - Localized string
localize_keyword(
    const char *attrname,		// I - Attribute name
    const char *keyword,		// I - Keyword string
    char       *buffer,			// I - String buffer
    size_t     bufsize)			// I - String buffer size
{
  char	*ptr;				// Pointer into string


  // TODO: Do real localization of keywords...
  if (!strcmp(keyword, "bi-level"))
  {
    strlcpy(buffer, "B&W (no shading)", bufsize);
  }
  else if (!strcmp(keyword, "monochrome"))
  {
    strlcpy(buffer, "B&W", bufsize);
  }
  else if (!strcmp(keyword, "main-roll"))
  {
    strlcpy(buffer, "Main", bufsize);
  }
  else if (!strcmp(keyword, "alternate-roll"))
  {
    strlcpy(buffer, "Alternate", bufsize);
  }
  else if (!strcmp(keyword, "labels"))
  {
    strlcpy(buffer, "Cut Labels", bufsize);
  }
  else if (!strcmp(keyword, "labels-continuous"))
  {
    strlcpy(buffer, "Continuous Labels", bufsize);
  }
  else if (!strcmp(attrname, "media-type") && !strcmp(keyword, "continuous"))
  {
    strlcpy(buffer, "Continuous Paper", bufsize);
  }
  else if (!strncmp(keyword, "photographic", 12))
  {
    if (keyword[12] == '-')
      snprintf(buffer, bufsize, "%c%s Photo Paper", toupper(keyword[13]), keyword + 14);
    else
      strlcpy(buffer, "Photo Paper", bufsize);
  }
  else if (!strcmp(keyword, "stationery"))
  {
    strlcpy(buffer, "Plain Paper", bufsize);
  }
  else if (!strcmp(keyword, "stationery-letterhead"))
  {
    strlcpy(buffer, "Letterhead", bufsize);
  }
  else if (!strcmp(keyword, "one-sided"))
  {
    strlcpy(buffer, "Off", bufsize);
  }
  else if (!strcmp(keyword, "two-sided-long-edge"))
  {
    strlcpy(buffer, "On (Portrait)", bufsize);
  }
  else if (!strcmp(keyword, "two-sided-short-edge"))
  {
    strlcpy(buffer, "On (Landscape)", bufsize);
  }
  else if (!strcmp(attrname, "media"))
  {
    pwg_media_t *pwg = pwgMediaForPWG(keyword);
					// PWG media size info

    if (!strcmp(pwg->ppd, "Letter"))
      strlcpy(buffer, "US Letter", bufsize);
    else if (!strcmp(pwg->ppd, "Legal"))
      strlcpy(buffer, "US Legal", bufsize);
    else if (!strcmp(pwg->ppd, "Env10"))
      strlcpy(buffer, "#10 Envelope", bufsize);
    else if (!strcmp(pwg->ppd, "A4") || !strcmp(pwg->ppd, "A5") || !strcmp(pwg->ppd, "A6"))
      strlcpy(buffer, pwg->ppd, bufsize);
    else if (!strcmp(pwg->ppd, "EnvDL"))
      strlcpy(buffer, "DL Envelope", bufsize);
    else if ((pwg->width % 100) == 0 && (pwg->width % 2540) != 0)
      snprintf(buffer, bufsize, "%d x %dmm", pwg->width / 100, pwg->length / 100);
    else
      snprintf(buffer, bufsize, "%g x %g\"", pwg->width / 2540.0, pwg->length / 2540.0);
  }
  else
  {
    strlcpy(buffer, keyword, bufsize);
    *buffer = (char)toupper(*buffer);
    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = (char)toupper(*ptr);
      }
    }
  }

  return (buffer);
}


//
// 'localize_media()' - Localize media-col information.
//

static char *				// O - Localized description of the media
localize_media(
    pappl_media_col_t *media,		// I - Media info
    bool              include_source,	// I - Include the media source?
    char              *buffer,		// I - String buffer
    size_t            bufsize)		// I - Size of string buffer
{
  char		size[128],		// I - Size name string
		source[128],		// I - Source string
		type[128];		// I - Type string


  if (!media->size_name[0])
    strlcpy(size, "Unknown", sizeof(size));
  else
    localize_keyword("media", media->size_name, size, sizeof(size));

  if (!media->type[0])
    strlcpy(type, "Unknown", sizeof(type));
  else
    localize_keyword("media-type", media->type, type, sizeof(type));

  if (include_source)
    snprintf(buffer, bufsize, "%s (%s) from %s", size, type, localize_keyword("media-source", media->source, source, sizeof(source)));
  else
    snprintf(buffer, bufsize, "%s (%s)", size, type);

  return (buffer);
}


//
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t         *client,	// I - Client
    pappl_pr_driver_data_t *driver_data,// I - Driver data
    const char             *title,	// I - Label/title
    const char             *name,	// I - Base name
    pappl_media_col_t      *media)	// I - Current media values
{
  int		i,			// Looping var
		cur_index = 0,		// Current size index
	        sel_index = 0;		// Selected size index...
  pwg_media_t	*pwg;			// PWG media size info
  char		text[256];		// Human-readable value/text
  const char	*min_size = NULL,	// Minimum size
		*max_size = NULL;	// Maximum size


  // media-size
  papplClientHTMLPrintf(client, "              <tr><th>%s Media:</th><td>", title);
  for (i = 0; i < driver_data->num_media && (!min_size || !max_size); i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];
    }
  }
  if (min_size && max_size)
  {
    papplClientHTMLPrintf(client, "<select name=\"%s-size\" onChange=\"show_hide_custom('%s');\"><option value=\"custom\">Custom Size</option>", name, name);
    cur_index ++;
  }
  else
    papplClientHTMLPrintf(client, "<select name=\"%s-size\">", name);

  for (i = 0; i < driver_data->num_media; i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];

      continue;
    }

    if (!strcmp(driver_data->media[i], media->size_name))
      sel_index = cur_index;

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->media[i], sel_index == cur_index ? " selected" : "", localize_keyword("media", driver_data->media[i], text, sizeof(text)));
    cur_index ++;
  }
  if (min_size && max_size)
  {
    int cur_width, min_width, max_width;// Current/min/max width
    int cur_length, min_length, max_length;
					// Current/min/max length

    if ((pwg = pwgMediaForPWG(min_size)) != NULL)
    {
      min_width  = pwg->width;
      min_length = pwg->length;
    }
    else
    {
      min_width  = 1 * 2540;
      min_length = 1 * 2540;
    }

    if ((pwg = pwgMediaForPWG(max_size)) != NULL)
    {
      max_width  = pwg->width;
      max_length = pwg->length;
    }
    else
    {
      max_width  = 9 * 2540;
      max_length = 22 * 2540;
    }

    if ((cur_width = media->size_width) < min_width)
      cur_width = min_width;
    else if (cur_width > max_width)
      cur_width = max_width;

    if ((cur_length = media->size_length) < min_length)
      cur_length = min_length;
    else if (cur_length > max_length)
      cur_length = max_length;

    papplClientHTMLPrintf(client, "</select><div style=\"display: %s;\" id=\"%s-custom\"><input type=\"number\" name=\"%s-custom-width\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"Width inches\">x<input type=\"number\" name=\"%s-custom-length\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"Height inches\"></div>\n", sel_index == 0 ? "inline-block" : "none", name, name, min_width / 2540.0, max_width / 2540.0, cur_width / 2540.0, name, min_length / 2540.0, max_length / 2540.0, cur_length / 2540.0);
  }
  else
    papplClientHTMLPuts(client, "</select>\n");

  if (driver_data->borderless)
  {
    papplClientHTMLPrintf(client, "                <input type=\"checkbox\" name=\"%s-borderless\"%s>&nbsp;Borderless\n", name, (!media->bottom_margin && !media->left_margin && !media->right_margin && !media->top_margin) ? " checked" : "");
  }

  // media-left/top-offset (if needed)
  if (driver_data->left_offset_supported[1] || driver_data->top_offset_supported[1])
  {
    papplClientHTMLPuts(client, "                Offset&nbsp;");

    if (driver_data->left_offset_supported[1])
    {
      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s-left-offset\" min=\"%.1f\" max=\"%.1f\" step=\"0.1\" value=\"%.1f\">", name, driver_data->left_offset_supported[0] / 100.0, driver_data->left_offset_supported[1] / 100.0, media->left_offset / 100.0);

      if (driver_data->top_offset_supported[1])
        papplClientHTMLPuts(client, "&nbsp;x&nbsp;");
    }

    if (driver_data->top_offset_supported[1])
      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s-top-offset\" min=\"%.1f\" max=\"%.1f\" step=\"0.1\" value=\"%.1f\">", name, driver_data->top_offset_supported[0] / 100.0, driver_data->top_offset_supported[1] / 100.0, media->top_offset / 100.0);

    papplClientHTMLPuts(client, "&nbsp;mm\n");
  }

  // media-tracking (if needed)
  if (driver_data->tracking_supported)
  {
    papplClientHTMLPrintf(client, "                <select name=\"%s-tracking\">", name);
    for (i = PAPPL_MEDIA_TRACKING_CONTINUOUS; i <= PAPPL_MEDIA_TRACKING_WEB; i *= 2)
    {
      const char *val = _papplMediaTrackingString((pappl_media_tracking_t)i);

      if (!(driver_data->tracking_supported & i))
	continue;

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, (pappl_media_tracking_t)i == media->tracking ? " selected" : "", localize_keyword("media-tracking", val, text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</select>\n");
  }

  // media-type
  papplClientHTMLPrintf(client, "                <select name=\"%s-type\">", name);
  for (i = 0; i < driver_data->num_type; i ++)
  {
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->type[i], !strcmp(driver_data->type[i], media->type) ? " selected" : "", localize_keyword("media-type", driver_data->type[i], text, sizeof(text)));
  }
  papplClientHTMLPrintf(client, "</select></td></tr>\n");
}


//
// 'time_string()' - Return the local time in hours, minutes, and seconds.
//

static char *
time_string(time_t tv,			// I - Time value
            char   *buffer,		// I - Buffer
	    size_t bufsize)		// I - Size of buffer
{
  struct tm	date;			// Local time and date

  localtime_r(&tv, &date);

  strftime(buffer, bufsize, "%X", &date);

  return (buffer);
}