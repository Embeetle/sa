// Copyright 2018-2024 Johan Cockx
#include "File.h"
#include "Project.h"
#include "Symbol.h"
#include "LocalSymbol.h"
#include "Hdir.h"
#include "Unit.h"
#include "Occurrence.h"
#include "Inclusion.h"
#include "FlagExtractor.h"
#include "Diagnostic.h"
#include "Range.h"
#include "FileKind.h"
#include "base/Timer.h"
#include "base/filesystem.h"
#include "base/string_util.h"
#include <iostream>
#include <ctype.h>
#include "base/debug.h"

sa::File::File(Project *project, const std::string &path)
  : Entity(EntityKind_file, project)
  , file_kind(guess_gcc_file_kind(path))
  , path(path)
{
  trace("create file " << this << " " << get_name() << " " << file_kind);
  assert(base::is_absolute_path(path));
  assert(base::is_normalized_path(path));
}

sa::File::~File()
{
  trace_nest("destroy " << this << " " << get_name() << " mode=" << _mode);
  assert(project->is_locked());
  for (unsigned kind = NumberOfOccurrenceKinds; kind--; ) {
    assert_(_occurrence_map[kind].empty(),
      kind << " " << _occurrence_map[kind].begin()->second
      << " " << *_occurrence_map[kind].begin()->second
    );
  }
  assert(_local_symbols.empty());
  assert(_diagnostic_map.empty());
  assert(_includers.empty());
  assert(!get_user_data());
}

std::string sa::File::get_key() const
{
  return project->get_unique_path(path);
}

std::string sa::File::get_build_path() const
{
  return project->get_build_path(path);
}

std::string sa::File::get_name() const
{
  return project->get_natural_path(path);
}

void sa::File::zero_ref_count()
{
  trace_nest("zero ref count -> remove file " << get_name());
  project->erase_file(this);
  Entity::zero_ref_count();
}

#ifdef CHECK
void sa::File::notify_ref_count() const
{
  trace("change ref count for " << this << " " << get_name() << " to "
    << get_ref_count()
  );
}
#endif

void sa::File::set_user_data(void *data)
{
  trace_nest("File set user data to " << data << " for " << get_name());
  if (data != user_data) {
    // Make sure that the file is never deleted while it has user data.
    if (!data != !user_data) {
      if (data) {
        increment_known();
      } else {
        decrement_known();
      }
    }
    user_data = data;
  }
}

const std::vector<base::ptr<sa::Section>> &sa::File::get_sections() const
{
  if (unit) {
    return unit->get_sections();
  } else {
    static std::vector<base::ptr<sa::Section>> no_sections;
    return no_sections;
  }
}

bool sa::File::is_in_project_folder() const
{
  return base::is_nested_in(path, project->get_project_path());
}

std::string sa::File::get_directory() const
{
  return base::get_parent_path(get_path());
}
 
sa::FileMode sa::File::get_mode() const
{
  assert(project->is_locked());
  return _mode;
}

const Vector<sa::Occurrence*> &sa::File::get_includers() const
{
  assert(project->is_locked());
  return _includers.members();
}

bool sa::File::is_linked() const
{
  return unit ? unit->is_linked() : false;
}

bool sa::File::is_in_link_command() const
{
  return unit ? unit->is_in_link_command() : false;
}

void sa::File::insert_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_first_instance,
  bool is_first_linked_instance
)
{
  // Insert a #include instantiation of this file.
  trace_nest("File::insert_instance_of_entity linked=" << linked << " "
    << *occurrence << " " << is_first_instance << "/" << is_first_linked_instance
    << " (old inclusion count " << inclusion_count << ")"
  );
  assert(project->is_locked());
  assert(occurrence->kind == OccurrenceKind_include);
  assert(occurrence->entity == this);
  if (is_first_instance) {
    assert(occurrence);
    assert(occurrence->kind == OccurrenceKind_include);
    increment_known();
    _includers.insert(occurrence);
  }
  if (linked) {
    inc_inclusion_count("insert/remove #include");
  }
}

void sa::File::remove_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_last_instance,
  bool is_last_linked_instance
)
{
  // Remove a #include instantiation of this file.
  trace_nest("File::remove_instance_of_entity linked=" << linked << " "
    << *occurrence << " " << is_last_instance << "/" << is_last_linked_instance
    << " (old inclusion count " << inclusion_count << ")"
  );
  assert(project->is_locked());
  assert(occurrence->kind == OccurrenceKind_include);
  assert(occurrence->entity == this);
  if (linked) {
    dec_inclusion_count("insert/remove #include");
  }
  if (is_last_instance) {
    assert(occurrence);
    assert(occurrence->kind == OccurrenceKind_include);
    _includers.remove(occurrence);
    decrement_known();
  }
}

void sa::File::track_occurrences_in_file(
  OccurrenceKindSet occurrence_kinds,
  EntityKindSet entity_kinds
)
{
  trace_nest("track occurrences " << occurrence_kinds << " " << entity_kinds
    << " in " << get_name()
  );
  assert(project->is_locked());
  enable_analysis("occurrences tracked in file");
  // If the set of entity kinds is empty, the set of occurrence kinds is
  // irrelevant. Make it empty to speed up the code below and to ease testing
  // for no tracked occurrences.
  if (entity_kinds.none()) {
    occurrence_kinds.reset();
  }
  bool old_tracked = tracked_occurrence_kinds.any()
    && tracked_entity_kinds.any();
  bool new_tracked = occurrence_kinds.any() && entity_kinds.any();
  if (old_tracked != new_tracked) {
    if (new_tracked) {
      increment_known();
    } else {
      decrement_known();
    }
  }
  if (occurrence_kinds.none() || tracked_occurrence_kinds.none()
    || entity_kinds == tracked_entity_kinds
  ) {
    // Optimized code: only handle occurrence kinds that have changed.  Why is
    // this allowed?
    //
    // - If there were previously no tracked occurrences, it is sufficient to
    //   update occurrence kinds that must now be tracked.
    //
    // - If there will now be no tracked occurrences, it is sufficient to
    //   update occurrences that used to be tracked
    //
    // - If the set of tracked entity kinds is unchanged, the only change can
    //   come from a changed set of occurrence kinds, so only occurrence kinds
    //   for which there is a change in that set can be affected.
    //
    // Note that to check that there are no tracked occurrences, it is
    // sufficient to check that the set of occurrence kinds is empty, because
    // the code above will force that set to be empty when the set of entity
    // kinds is empty.
    trace("optimized");
    for (unsigned okind = NumberOfOccurrenceKinds; okind--; ) {
      const bool track_okind_old = tracked_occurrence_kinds[okind];
      const bool track_okind_new = occurrence_kinds[okind];
      if (track_okind_old != track_okind_new) {
        if (track_okind_new) {
          trace(" `-> add " << (OccurrenceKind)okind);
          for (auto const &[key, occurrence]: _occurrence_map[okind]) {
            (void)key;
            const auto ekind = occurrence->entity->kind;
            if (entity_kinds[ekind] && occurrence->is_instantiated()) {
              trace("track " << *occurrence);
              add_tracked_occurrence(occurrence);
            } else {
              trace("do not track " << *occurrence << ": "
                << occurrence->is_instantiated()
                << " " << ekind << " " << entity_kinds[ekind]
              );
            }
          }
        } else {
          trace(" `-> remove " << (OccurrenceKind)okind);
          for (auto const &[key, occurrence]: _occurrence_map[okind]) {
            (void)key;
            const auto ekind = occurrence->entity->kind;
            if (tracked_entity_kinds[ekind] && occurrence->is_instantiated()) {
              remove_tracked_occurrence(occurrence);
            }
          }
        }
      }
    }
  } else {
    // General code: both the set of occurrence kinds and the set of entity
    // kinds have changed, and neither of them is empty.
    trace("not optimized");
    for (unsigned okind = NumberOfOccurrenceKinds; okind--; ) {
      const bool track_okind_old = tracked_occurrence_kinds[okind];
      const bool track_okind_new = occurrence_kinds[okind];
      if (track_okind_old || track_okind_new) {
        trace(" `-> handle " << (OccurrenceKind)okind);
        for (auto const &[key, occurrence]: _occurrence_map[okind]) {
          (void)key;
          const auto ekind = occurrence->entity->kind;
          const bool track_old = track_okind_old && tracked_entity_kinds[ekind];
          const bool track_new = track_okind_new && entity_kinds[ekind];
          if (track_old != track_new && occurrence->is_instantiated()) {
            if (track_new) {
              add_tracked_occurrence(occurrence);
            } else {
              remove_tracked_occurrence(occurrence);
            }
          }
        }
      }
    }
  }
  tracked_entity_kinds = entity_kinds;
  tracked_occurrence_kinds = occurrence_kinds;
}

void sa::File::insert_empty_loop_in_file(EmptyLoop *empty_loop)
{
  trace("insert empty loop " << *empty_loop);
  _empty_loops.insert(empty_loop);
}
    
void sa::File::remove_empty_loop_in_file(EmptyLoop *empty_loop)
{
  trace("remove empty loop " << *empty_loop);
  _empty_loops.remove(empty_loop);
}

sa::Range sa::File::find_empty_loop(unsigned offset)
{
  for (auto loop: _empty_loops.members()) {
    const Range &range = loop->get_range();
    if (range.contains(offset)) {
      return range;
    }
  }
  return Range(0,0);
}

void sa::File::insert_occurrence_in_file(Occurrence *occurrence)
{
  trace_nest("insert occurrence in file: " << *occurrence);
  if (tracked_occurrence_kinds[occurrence->kind]
    && tracked_entity_kinds[occurrence->entity->kind]
  ) {
    trace("add tracked " << *occurrence
      << " " << occurrence->entity->get_effective_kind()
    );
    add_tracked_occurrence(occurrence);
  }
}

void sa::File::remove_occurrence_in_file(Occurrence *occurrence)
{
  trace("remove occurrence in file: " << *occurrence);
  if (tracked_occurrence_kinds[occurrence->kind]
    && tracked_entity_kinds[occurrence->entity->kind]
  ) {
    trace("remove tracked " << *occurrence
      << " " << occurrence->entity->get_effective_kind()
    );
    remove_tracked_occurrence(occurrence);
  }
}

void sa::File::update_occurrence_in_file(Occurrence *occurrence)
{
  trace_nest("update occurrence in file: " << *occurrence);
  if (tracked_occurrence_kinds[occurrence->kind]
    && tracked_entity_kinds[occurrence->entity->kind]
  ) {
    trace("update tracked " << *occurrence
      << " " << occurrence->entity->get_effective_kind()
    );
    update_tracked_occurrence(occurrence);
  }
}

void sa::File::add_tracked_occurrence(Occurrence *occurrence)
{
  trace("add tracked occurrence " << *occurrence);
  project->add_occurrence_in_file(occurrence);
}

void sa::File::remove_tracked_occurrence(Occurrence *occurrence)
{
  trace("remove tracked occurrence " << *occurrence);
  project->remove_occurrence_in_file(occurrence);
}

void sa::File::update_tracked_occurrence(Occurrence *occurrence)
{
  trace("update tracked occurrence " << *occurrence);
  project->update_occurrence_in_file(occurrence);
}

void sa::File::track_occurrences_of_entity(
  OccurrenceKindSet occurrence_kinds
)
{
  trace_nest("track occurrences " << occurrence_kinds
    << " of " << get_entity_name()
  );
  assert(project->is_locked());

  if (occurrence_kinds.test(OccurrenceKind_include) != tracked) {
    tracked = !tracked;
    if (tracked) {
      for (auto occ: get_includers()) {
        trace_nest("add " << occ << " " << *occ);
        project->add_occurrence_of_entity(occ);
      }
    } else {
      for (auto occ: get_includers()) {
        trace_nest("remove " << occ << " " << *occ);
        project->remove_occurrence_of_entity(occ);
      }
    }
  }    
}

bool sa::File::is_link_candidate() const
{
  return has_linkable_file_kind() && (
    _mode == FileMode_include
    || ( _mode == FileMode_automatic && file_kind != FileKind_object)
    || is_in_link_command()
  );
}

void sa::File::set_mode(FileMode new_mode)
{
  assert(project->is_locked());
  trace_nest("set_mode " << new_mode << " for " << get_name());
  if (_mode != new_mode) {
    trace(" `-> change mode from " << _mode << " to " << new_mode
      << " for " << get_name()
    );
    // Careful: changing the file mode can change the project's pending file
    // count in two ways:
    //
    //  - due to a change in link candidate status, and
    //
    //  - due to a change in analysis status when a unit is created.
    //
    // To avoid double changes to the pending file count, these effects must not
    // overlap.
    //
    // Inc/dec link count has no effect on the pending file count (as it only
    // affects the reported link status), so can be done independently.
    //
    if (has_linkable_file_kind()) {
      // Note: changing to mode=include always immediately marks the file as
      // included, but changing to mode=exclude does *not* always immediately
      // mark the file as excluded, because the file might still occur on the
      // linker command line.
      if (new_mode == FileMode_include) {
        inc_link_count();
      }
      if (_mode == FileMode_include) {
        dec_link_count();
      }
    }
    // Let's first change the file mode. If the file's analysis status is not a
    // pending status (i.e. not waiting or busy), then this has no effect on the
    // number of pending files.
    bool old_is_link_candidate = is_link_candidate();
    _mode = new_mode;
    bool new_is_link_candidate = is_link_candidate();
    if (old_is_link_candidate != new_is_link_candidate) {
      project->update_link_candidate_status(
        this, new_is_link_candidate, get_analysis_status()
      );
    }
    // Now let's create a unit if necessary, which may affect the pending
    // status.  A link candidate *must* be analyzed to allow the linker to work.
    // Another file with linkable kind *can* be analyzed; if it is, update
    // mode. Don't create unit until find_occurrence or track_occurrences is
    // called.
    if (new_is_link_candidate && !unit) {
      trace_nest("create new unit for " << get_name());
      unit = base::ptr<Unit>::create(this);
    }
    if (unit) {
      trace_nest("set unit mode");
      unit->set_mode(new_mode);
    }
  }
}

bool sa::File::is_tracked() const
{
  return tracked_occurrence_kinds.any() && tracked_entity_kinds.any();
}

void sa::File::enable_analysis(const char *reason)
{
  trace_nest("enable analysis for " << get_name() << " because " << reason);
  assert(project->is_locked());
  if (!unit && has_linkable_file_kind()) {
    trace("create new analyzer for " << get_path());
    unit = base::ptr<Unit>::create(this);
    unit->set_mode(_mode);
  }
}

void sa::File::request_update(const char *reason)
{
  assert(project->is_locked());
  trace_nest(
    "File::request_update for " << get_name() << " because " << reason
  );
  trace(" `-> _mode=" << _mode << " unit=" << unit << " is-in-link-command="
    << (unit && unit->is_in_link_command())
  );
  if (unit) {
    unit->trigger();
  }
}

void sa::File::notify_out_of_date_and_unblock_unit(const char *reason)
{
  assert(project->is_locked());
  trace_nest("File::notify_out_of_date for " << get_name() << " because "
    << reason
  );
  trace(" `-> _mode=" << _mode << " unit=" << unit << " is-in-link-command="
    << (unit && unit->is_in_link_command())
    << " is-up-to-date=" << (!unit || unit->is_up_to_date())
  );
  if (unit) {
    if (_mode == FileMode_exclude && unit->is_up_to_date()) {
      project->remove_unit(unit);
      unit = 0;
    } else {
      Unit::Lock lock(unit);
      unit->_trigger();
      unit->_unblock();
      trace("unblocked, new block count=" << unit->_get_block_count());
    }
  }
}

void sa::File::notify_flags_out_of_date(const char *reason)
{
  assert(project->is_locked());
  trace_nest("File::notify_flags_out_of_date for " << get_name() << " because "
    << reason
  );
  trace(" `-> _mode=" << _mode << " unit=" << unit << " is-in-link-command="
    << (unit && unit->is_in_link_command())
    << " is-up-to-date=" << (!unit || unit->is_up_to_date())
  );
  if (unit) {
    if (_mode == FileMode_exclude && unit->is_up_to_date()) {
      project->remove_unit(unit);
      unit = 0;
    } else {
      project->flag_extractor->update_target(unit);
      //
      // It is unnecessary to re-analyze this unit unless flags have changed,
      // but I need to trigger it anyway to make sure that the project becomes
      // busy and any client will wait until flag extraction is complete. I need
      // a better synchronisation mechanism!!!
      unit->trigger();
    }
  }
}

void sa::File::request_flags_update(const char *reason)
{
  assert(project->is_locked());
  trace_nest("File::request_flags_update for " << get_name() << " because "
    << reason
  );
  trace(" `-> _mode=" << _mode << " unit=" << unit << " is-in-link-command="
    << (unit && unit->is_in_link_command())
  );
  if (unit) {
    project->flag_extractor->update_target(unit);
  }
}

void sa::File::reload(const char *reason)
{
  trace_nest("reload file " << get_name());
  for (auto section: get_sections()) {
    trace("has section " << *section);
  }
  assert(project->is_locked());
  apply_edits();
  std::set<File*> files;
  add_to_set_with_includers(files);
  std::string reason2 = reason + std::string(" for included file");
  for (auto file: files) {
    file->request_update(file==this ? reason : reason2.data());
  }
  project->reload_as_config_file(this);
}

void sa::File::add_to_set_with_includers(
  std::set<File*> &files
)
{
  trace_nest("add includers for " << get_name());
  assert(project->is_locked());
  if (files.find(this) == files.end()) {
    trace("insert " << *this);
    files.insert(this);
    const Vector<Occurrence*> includers = get_includers();
    for (auto occurrence: includers) {
      occurrence->file->add_to_set_with_includers(files);
    }
  }
}

// Determine if a range is a better match than the reference range when looking
// for occurrence data. Smaller ranges are considered better because more
// specific.
static bool is_better_match(const sa::Range &range, const sa::Range &ref)
{
  return range.end - range.begin < ref.end - ref.begin;
}

void sa::File::edit(Range range, const char *new_content)
{
  trace("File::edit " << range << " '" << new_content << "' in " << get_name());
  assert(project->is_locked());
  edit_log.edit(range, new_content);
}

void sa::File::apply_edits()
{
  trace_nest("File::apply_edits " << get_name());
  assert(project->is_locked());
  for (unsigned kind = NumberOfOccurrenceKinds; kind--; ) {
    std::map<OccurrenceKey, Occurrence*> new_map;
    for (auto const &[old_key, occurrence]: _occurrence_map[kind]) {
      assert(occurrence->range == old_key.range);
      occurrence->range = edit_log.apply(occurrence->range);
      if (occurrence->range.is_void()) {
        // Don't keep empty occurrences.  Multiple initially different
        // occurrences might end up equal, causing trouble in the map.
        trace("drop " << *occurrence << " from " << old_key.range);
      } else {
        if (occurrence->range != old_key.range) {
          trace("move " << *occurrence << " from " << old_key.range);
        }
        OccurrenceKey new_key = {
          occurrence->range,
          occurrence->entity,
          occurrence->get_hdir(),
          occurrence->kind,
          occurrence->style,
        };
        new_map.insert(std::make_pair(new_key, occurrence));
      }
    }
    _occurrence_map[kind].swap(new_map);
  }
  // Reference occurrence and scope of local symbols may also have changed, so
  // rebuild set of local symbols.
  base::ptrset<LocalSymbol*> local_symbols;
  for (auto symbol: _local_symbols) {
    symbol->ref_location.offset = edit_log.apply(symbol->ref_location.offset);
    local_symbols.insert(symbol);
  }
  _local_symbols.swap(local_symbols);
  edit_log.clear();
}

sa::OccurrenceData sa::File::find_occurrence_data(
  unsigned offset, unsigned begin_tol, unsigned end_tol
)
{
  trace("get occurrence data at " << get_name() << "@" << offset << "#-"
    << begin_tol << "..+" << end_tol
  );
  assert(project->is_locked());
  enable_analysis("occurrence data requested");
  const unsigned M = (unsigned)-1;
  Range click_range = edit_log.revert(
    Range(
      end_tol < offset ? offset - end_tol : 0,
      offset < M - begin_tol - 1 ? offset + begin_tol + 1 : M
    )
  );
  Occurrence *best_match = 0;
  Range best_match_range;
  if (!click_range.is_empty()) {
    for (unsigned kind = NumberOfOccurrenceKinds; kind--; ) {
      for (auto const &[key, occurrence]: _occurrence_map[kind]) {
        (void)key;
        trace("try " << *occurrence);
        if (occurrence->is_instantiated()) {
          if (click_range.overlaps(occurrence->range)) {
            if (!best_match
              || is_better_match(occurrence->range, best_match->range)
            ) {
              Range post_edit_range = edit_log.apply(occurrence->range);
              if (!post_edit_range.is_empty()) {
                best_match = occurrence;
                best_match_range = post_edit_range;
                trace("found " << *occurrence);
              }
            }
          }
        }
      }
    }
  }

  OccurrenceData data;
  if (best_match) {
    trace("return " << *best_match);
    best_match->entity->increment_known();
    data = {
      .kind =             best_match->kind,
      .entity_user_data = best_match->entity->get_user_data(),
      .entity =           best_match->entity,
      .path =             best_match->file->get_path().data(),
      .begin_offset =     best_match_range.begin,
      .end_offset =       best_match_range.end,
      .linked =           best_match->is_linked(),
    };
  } else {
    trace("return null");
    data = null_occurrence_data;
  }
  return data;
}

static const char *get_trailing_id(const char *context)
{
  const char *p = context + strlen(context);
  while ((isalnum(p[-1]) || p[-1] == '_') && p > context) --p;
  while (isdigit(*p)) ++p;
  return p;
}

unsigned sa::File::get_completions(
  unsigned pos,
  void (*add_completion)(const char *completion, void *user_data),
  void *user_data,
  const char *context
)
{
  trace("get completions at " << get_name() << "@" << pos
    << " context='" << context << "'"
  );
  assert(project->is_locked());
  enable_analysis("completion data requested");
  std::string const &prefix = get_trailing_id(context);
  std::set<Entity*> done;
  get_completions_here( prefix, add_completion, user_data, pos, done);
  return pos - prefix.size();
}

void sa::File::get_completions_here(
  std::string const &prefix,
  void (*add_completion)(const char *completion, void *user_data),
  void *user_data,
  unsigned pos,
  std::set<Entity*> &done
)
{
  (void)prefix;
  if (done.find(this) != done.end()) {
    return;
  }
  done.insert(this);
  for (auto const &[key, occurrence]: _occurrence_map[OccurrenceKind_include]) {
    (void)key;
    occurrence->entity->get_file()->get_completions_here(
      prefix, add_completion, user_data, (unsigned)-1, done
    );
  }
  for (unsigned kind = NumberOfDeclaringOccurrenceKinds; kind--; ) {
    for (auto const &[key, occurrence]: _occurrence_map[kind]) {
      (void)key;
      trace("try " << *occurrence);
      if (occurrence->is_instantiated() && pos > occurrence->get_range().end
        && occurrence->entity->kind != EntityKind_field
        && occurrence->entity->kind != EntityKind_parameter
        && occurrence->entity->kind != EntityKind_automatic_variable
        && occurrence->entity->kind != EntityKind_local_static_variable
        && occurrence->entity->kind != EntityKind_section
      ) {
        Symbol *symbol = occurrence->entity->as_symbol();
        if (done.find(symbol) == done.end()) {
          done.insert(symbol);
          std::string const &name = symbol->get_name();
          if (base::begins_with(name, prefix)) {
            add_completion(name.data(), user_data);
          }
        }
      }
    }
  }
}

base::ptr<sa::Occurrence> sa::File::get_occurrence(
  OccurrenceKind kind,
  OccurrenceStyle style,
  const base::ptr<Entity> &entity,
  const Range &range,
  const base::ptr<Hdir> &hdir
)
{
  Project::Lock lock(project);

  trace_nest("get " << kind << " of " << entity << " " << *entity
    << " as " << style
    << " in " << this << " " << get_name() << " at " << range
    << " hdir=" << hdir
  );
  check_code(check());
  OccurrenceKey key = { range, entity, hdir, kind, style };
  auto it = _occurrence_map[kind].find(key);
  if (it != _occurrence_map[kind].end()) {
    trace(" `-> reuse " << kind << " of " << entity->get_entity_name()
      << " at " << range
    );
    assert_(base::is_valid(it->second),
      "invalid occurrence in map of " << this << " " << get_name()
    );
    return it->second;
  }
  trace(" `-> create " << kind << " of " << entity->get_entity_name()
    << " at " << get_name() << "." << range
    << " hdir=" << (hdir ? hdir->path : "<none>")
  );
  auto occurrence = kind == OccurrenceKind_include
    ? base::ptr<Inclusion>::create(
      entity.static_cast_to<File>(), this, range, hdir
    ).static_cast_to<Occurrence>()
    : base::ptr<Occurrence>::create(kind, style, entity, this, range)
    ;
  _occurrence_map[kind].insert(std::make_pair(key, occurrence));
  check_code(check());
  //trace("created and inserted " << occurrence << " " << *occurrence);
  return occurrence;
}

void sa::File::drop_occurrence(Occurrence *occurrence)
{
  assert(base::is_valid(occurrence));
  OccurrenceKind kind = occurrence->kind;
  trace_nest("Drop occurrence " << occurrence << " " << *occurrence
    << " from map of " << this << " " << get_name()
    << " (" << _occurrence_map[kind].size() << " occs)"
  );
  assert(project->is_locked());
  check_code(check());
  OccurrenceKey key = {
    occurrence->range,
    occurrence->entity,
    occurrence->get_hdir(),
    occurrence->kind,
    occurrence->style,
  };
  // Occurrences with void range are no longer in the occurrence map; don't try
  // to remove them again. A range can become void when the source file is
  // edited (see EditLog.h).
  if (!occurrence->range.is_void()) {
    assert_(_occurrence_map[kind].find(key) != _occurrence_map[kind].end(),
      *occurrence 
    );
    _occurrence_map[kind].erase(key);
  }
  assert(_occurrence_map[kind].find(key) == _occurrence_map[kind].end());
  check_code(check());
}

base::ptr<sa::EmptyLoop> sa::File::get_empty_loop(const Range &range)
{
  Project::Lock lock(project);
  trace_nest("get empty loop in " << " " << get_name() << " at " << range);
  assert(!range.is_empty());
  auto it = _empty_loop_map.find(range);
  if (it != _empty_loop_map.end()) {
    trace(" `-> reuse empty loop at " << range);
    assert_(base::is_valid(it->second),
      "invalid empty loop in map of " << get_name()
    );
    return it->second;
  }
  trace(" `-> create empty loop at " << get_name() << "." << range);
  auto empty_loop = base::ptr<EmptyLoop>::create(this, range);
  _empty_loop_map.insert(std::make_pair(range, empty_loop));
  return empty_loop;
}

void sa::File::drop_empty_loop(EmptyLoop *empty_loop)
{
  assert(base::is_valid(empty_loop));
  Range range = empty_loop->get_range();
  trace_nest("Drop empty loop at " << range << " from map of " << get_name());
  assert(project->is_locked());
  assert_(_empty_loop_map.find(range) != _empty_loop_map.end(),
    get_name() << "." << range
  );
  _empty_loop_map.erase(range);
  assert(_empty_loop_map.find(range) == _empty_loop_map.end());
}

#ifdef CHECK
void sa::File::check() const
{
  for (const auto &e: _occurrence_map[OccurrenceKind_include]) {
    assert(base::is_valid(e.second));
  }
}
#endif

bool sa::File::OccurrenceKey::operator<(const OccurrenceKey &other) const
{
  if (range < other.range) return true;
  if (range > other.range) return false;
  if (entity < other.entity) return true;
  if (entity > other.entity) return false;
  if (hdir < other.hdir) return true;
  if (hdir > other.hdir) return false;
  if (kind < other.kind) return true;
  if (kind > other.kind) return false;
  return style < other.style;
}

base::ptr<sa::Diagnostic> sa::File::get_diagnostic(
  const std::string &message,
  Severity severity,
  const Location &location
)
{
  assert(project->is_locked());
  trace("get " << severity << ": " << message << " at " << get_name()
    << "." << location
  );
  const DiagnosticKey key = { message, severity, location };
  auto it = _diagnostic_map.find(key);
  if (it != _diagnostic_map.end()) {
    trace("reuse");
    assert(base::is_valid(it->second));
    return it->second;
  }
  trace("create");
  auto diagnostic = base::ptr<Diagnostic>::create(
    message, severity, Category_none, this, location
  );
  _diagnostic_map.insert(std::make_pair(key, diagnostic));
  return diagnostic;
}

void sa::File::drop_diagnostic(Diagnostic *diagnostic)
{
  assert(project->is_locked());
  assert(base::is_valid(diagnostic));
  trace("drop " << diagnostic->get_severity() << ": "
    << diagnostic->get_message() << " at " << diagnostic->location
  );
  const DiagnosticKey key = {
    diagnostic->get_message(),
    diagnostic->get_severity(),
    diagnostic->location,
  };
  _diagnostic_map.erase(key);
}

void sa::File::erase_local_symbol(LocalSymbol *symbol)
{
  trace("File erase local symbol " << symbol << " " << *symbol);
  assert(project->is_locked());
  assert(base::is_valid(symbol));
  _local_symbols.erase(symbol);
}

base::ptr<sa::LocalSymbol> sa::File::get_local_symbol(
  EntityKind kind,
  const std::string &user_name,
  unsigned ref_location,
  base::ptr<Occurrence> ref_scope
)
{
  // Create new symbol before locking the project, to keep the lock as short as
  // possible.
  base::ptr<LocalSymbol> new_symbol = base::ptr<LocalSymbol>::create(
    kind,
    user_name,
    FileLocation(this, ref_location),
    ref_scope,
    project
  );
  Project::Lock lock(project);
  trace_nest("File get local symbol " << new_symbol << " " << *new_symbol);
  base::ptr<LocalSymbol> symbol = _local_symbols.find_or_insert(new_symbol);
  trace("got " << (symbol == new_symbol ? "new" : "old") << " local symbol "
    << symbol << " " << *symbol
  );
  // If new symbol is not needed because there was already an equivalent symbol
  // in the set, make sure it is freed while the project is locked.
  new_symbol = 0;
  assert(base::is_valid(symbol));
  return symbol;
}

bool sa::File::DiagnosticKey::operator<(const DiagnosticKey &other) const
{
  if (severity < other.severity) return true;
  if (severity > other.severity) return false;
  if (message < other.message) return true;
  if (message > other.message) return false;
  return location < other.location;
}

std::ostream &sa::operator<<(std::ostream &out, const sa::FileLocation &location)
{
  assert(base::is_valid(location.file));
  out << location.file->get_name() << "." << (const Location&)location;
  return out;
}

bool sa::File::analysis_data_was_read_from_cache() const
{
  assert(project->is_locked());
  return unit && unit->was_read_from_cache();
}

void sa::File::inc_in_link_command_count()
{
  trace_nest("inc in link command count for " << get_name());
  assert(project->is_locked());
  inc_inclusion_count("inc/excl from link command");
  if (has_linkable_file_kind()) {
    enable_analysis("file occurs in link command");
    unit->inc_hard_link_count();
  }
}

void sa::File::dec_in_link_command_count()
{
  trace_nest("dec in link command count for " << get_name());
  assert(project->is_locked());
  if (has_linkable_file_kind()) {
    // Assert below failed for Kristof, cannot reproduce, happened after removing
    // config/rom.o in the Atmosic project.
    assert_(base::is_valid(unit), get_name() << " " << file_kind);
    unit->dec_hard_link_count();
    // No need to disable analysis; existing analysis, if any, may be useful.
  }
  dec_inclusion_count("inc/excl from link command");
}

void sa::File::inc_inclusion_count(const char *reason)
{
  trace_nest("inc inclusion count to " << (inclusion_count+1) << " for "
    << get_name() << ": " << reason
  );
  assert(project->is_locked());
  if (!inclusion_count) {
    project->report_file_inclusion_status(this, 1);
  }
  ++inclusion_count;
  assert(inclusion_count);
}

void sa::File::dec_inclusion_count(const char *reason)
{
  trace_nest("dec inclusion count to " << (inclusion_count-1) << " for "
    << get_name() << ": " << reason
  );
  assert(project->is_locked());
  assert(inclusion_count);
  --inclusion_count;
  if (!inclusion_count) {
    project->report_file_inclusion_status(this, 0);
  }
}

void sa::File::inc_link_count()
{
  trace_nest("inc link count to " << (link_count+1) << " for " << get_name());
  assert(project->is_locked());
  if (!link_count) {
    project->report_file_link_status(this, 1);
  }
  ++link_count;
  assert(link_count);
}

void sa::File::dec_link_count()
{
  trace_nest("dec link count to " << (link_count-1) << " for " << get_name());
  assert(project->is_locked());
  assert(link_count);
  --link_count;
  if (!link_count) {
    project->report_file_link_status(this, 0);
  }
}

void sa::File::increment_non_utf8()
{
  trace_nest("inc non-utf8 count to " << (non_utf8_count+1)
    << " for " << get_name()
  );
  assert(project->is_locked());
  if (!non_utf8_count) {
    project->report_utf8(this, false);
  }
  ++non_utf8_count;
  assert(non_utf8_count);
}

void sa::File::decrement_non_utf8()
{
  trace_nest("dec non-utf8 count to " << (non_utf8_count-1)
    << " for " << get_name()
  );
  assert(project->is_locked());
  assert(non_utf8_count);
  --non_utf8_count;
  if (!non_utf8_count) {
    project->report_utf8(this, true);
  }
}

void sa::File::update_status_if_used()
{
  assert(project->is_locked());
  if (inclusion_count) {
    project->report_file_inclusion_status(this, 1);
  }
  if (link_count) {
    project->report_file_link_status(this, 1);
  }
  if (non_utf8_count) {
    project->report_utf8(this, false);
  }
}
  
#ifndef NDEBUG
void sa::File::increment_known()
{
  assert(project->is_locked());
  Entity::increment_known();
  ++known_count;
  trace("inc known to " << known_count << " for " << get_name());
  assert(known_count);
}

void sa::File::decrement_known()
{
  assert(project->is_locked());
  assert(known_count);
  --known_count;
  trace("dec known to " << known_count << " for " << get_name());
  Entity::decrement_known();
  // Don't access file members here: if known count is zero, the file was
  // deleted! Handle known_count before calling decrement_known!
}
#endif
