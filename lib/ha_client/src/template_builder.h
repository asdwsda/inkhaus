#pragma once
#include <map>
#include <string>
#include <vector>

// True for ids of the form "domain.object_id" (lowercase, digits, underscore).
// Ids are pasted into the Jinja template, so anything else is rejected.
bool is_valid_entity_id(const std::string& id);

// Domain part of an entity id ("" if there is no dot).
std::string entity_domain(const std::string& id);

// Renders the Jinja template for the given ids. Returns false (and sets error)
// if the list is empty or any id is invalid.
// extra: widget-requested attribute names per entity id (task 030), fetched in addition to the
// domain's fixed attributes. Names must match [a-z0-9_]+ (<= 40 chars) or the build fails.
using ExtraAttrs = std::map<std::string, std::vector<std::string>>;
bool build_state_template(const std::vector<std::string>& ids, std::string& out,
                          std::string& error, const ExtraAttrs& extra = ExtraAttrs());

// JSON body for POST /api/template: {"template": "<template>"}.
bool build_template_request_body(const std::vector<std::string>& ids, std::string& body,
                                 std::string& error, const ExtraAttrs& extra = ExtraAttrs());
