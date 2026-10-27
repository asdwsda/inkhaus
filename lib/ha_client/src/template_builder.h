#pragma once
#include <string>
#include <vector>

// True for ids of the form "domain.object_id" (lowercase, digits, underscore).
// Ids are pasted into the Jinja template, so anything else is rejected.
bool is_valid_entity_id(const std::string& id);

// Domain part of an entity id ("" if there is no dot).
std::string entity_domain(const std::string& id);

// Renders the Jinja template for the given ids. Returns false (and sets error)
// if the list is empty or any id is invalid.
bool build_state_template(const std::vector<std::string>& ids, std::string& out,
                          std::string& error);

// JSON body for POST /api/template: {"template": "<template>"}.
bool build_template_request_body(const std::vector<std::string>& ids, std::string& body,
                                 std::string& error);
