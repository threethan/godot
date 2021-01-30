/*************************************************************************/
/*  scroll_container.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "scroll_container.h"
#include "core/os/os.h"
#include "scene/main/viewport.h"
#include "core/os/keyboard.h"

bool ScrollContainer::clips_input() const {
	return true;
}

Size2 ScrollContainer::get_minimum_size() const {

	Ref<StyleBox> sb = get_stylebox("bg");
	Size2 min_size;

	for (int i = 0; i < get_child_count(); i++) {

		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c)
			continue;
		if (c->is_set_as_toplevel())
			continue;
		if (c == h_scroll || c == v_scroll)
			continue;
		Size2 minsize = c->get_combined_minimum_size();

		if (!scroll_h) {
			min_size.x = MAX(min_size.x, minsize.x);
		}
		if (!scroll_v) {
			min_size.y = MAX(min_size.y, minsize.y);
		}
	}

	if (h_scroll->is_visible_in_tree()) {
		min_size.y += h_scroll->get_minimum_size().y;
	}
	if (v_scroll->is_visible_in_tree()) {
		min_size.x += v_scroll->get_minimum_size().x;
	}
	min_size += sb->get_minimum_size();
	return min_size;
}

void ScrollContainer::_cancel_drag() {
	set_physics_process_internal(false);
	animating = false;
	drag_touching = false;
	drag_speed = Vector2();
	drag_accum = Vector2();
	drag_from = Vector2();

	if (beyond_deadzone) {
		emit_signal("scroll_ended");
		propagate_notification(NOTIFICATION_SCROLL_END);
		beyond_deadzone = false;
	}
}

void ScrollContainer::_start_inertial_scroll() {
	inertial_scroll_duration_current = inertial_time_left;
	inertial_start = Vector2(h_scroll->get_value(), v_scroll->get_value());
	animating = true;
	set_physics_process_internal(true);
}

void ScrollContainer::_button_scroll(bool p_horizontal, float p_amount) {
	// Cap the inertial target
	if (inertial_target.x < 0) {
		inertial_target.x = 0;
	}
	if (inertial_target.x > (h_scroll->get_max() - h_scroll->get_page())) {
		inertial_target.x = h_scroll->get_max() - h_scroll->get_page();
	}
	if (inertial_target.y < 0) {
		inertial_target.y = 0;
	}
	if (inertial_target.y > (v_scroll->get_max() - v_scroll->get_page())) {
		inertial_target.y = v_scroll->get_max() - v_scroll->get_page();
	}
	_cancel_drag();

	// Multiply the amount by the step (1/8th of a page)
	if (p_horizontal) {
		p_amount *= h_scroll->get_page() * scroll_step;
	} else {
		p_amount *= v_scroll->get_page() * scroll_step;
	}
	
	// Do scroll
	if (always_smoothed) {
		// Vector2 inertial_target = Vector2(h_scroll->get_value(), v_scroll->get_value());
		if (p_horizontal) {
			inertial_target.x += p_amount;
		} else {
			inertial_target.y += p_amount;
		}
		inertial_time_left = smooth_scroll_duration_button;
		_start_inertial_scroll();
		emit_signal("scroll_started");
		propagate_notification(NOTIFICATION_SCROLL_BEGIN);
	} else {
		if (p_horizontal) {
			h_scroll->set_value(h_scroll->get_value() + p_amount);
		} else {
			v_scroll->set_value(v_scroll->get_value() + p_amount);
		}
	}
}

void ScrollContainer::_gui_input(const Ref<InputEvent> &p_event) {

	double prev_v_scroll = v_scroll->get_value();
	double prev_h_scroll = h_scroll->get_value();

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->get_button_index() == BUTTON_WHEEL_UP && mb->is_pressed()) {
			// If only horizontal is enabled, scroll horizontally
			if (h_scroll->is_visible() && (!v_scroll->is_visible() || mb->get_shift())) {
				_button_scroll(true, -mb->get_factor());
			} else if (v_scroll->is_visible_in_tree()) {
				_button_scroll(false, -mb->get_factor());
			}
		}

		if (mb->get_button_index() == BUTTON_WHEEL_DOWN && mb->is_pressed()) {
			// If only horizontal is enabled, scroll horizontally
			if (h_scroll->is_visible() && (!v_scroll->is_visible() || mb->get_shift())) {
				_button_scroll(true, mb->get_factor());
			} else if (v_scroll->is_visible_in_tree()) {
				_button_scroll(false, mb->get_factor());
			}
		}

		if (mb->get_button_index() == BUTTON_WHEEL_LEFT && mb->is_pressed()) {
			if (h_scroll->is_visible_in_tree()) {
				_button_scroll(true, -mb->get_factor());
			}
		}

		if (mb->get_button_index() == BUTTON_WHEEL_RIGHT && mb->is_pressed()) {
			if (h_scroll->is_visible_in_tree()) {
				_button_scroll(true, mb->get_factor());
			}
		}

		if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
			accept_event(); //accept event if scroll changed

		if (!OS::get_singleton()->has_touchscreen_ui_hint())
			return;

		if (mb->get_button_index() != BUTTON_LEFT)
			return;

		if (mb->is_pressed()) {
			if (drag_touching) {
				_cancel_drag();
			}

			drag_speed = Vector2();
			drag_accum = Vector2();
			drag_from = Vector2(h_scroll->get_value(), v_scroll->get_value());
			drag_touching = OS::get_singleton()->has_touchscreen_ui_hint();
			if (drag_touching) {
				set_physics_process_internal(true);
			}
			animating = false;
			beyond_deadzone = false;

		} else {
			if (drag_touching) {
				drag_touching = false;
				inertial_time_left = inertial_scroll_duration_touch;
				_start_inertial_scroll();
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {

		if (drag_touching && !animating) {

			Vector2 motion = Vector2(mm->get_relative().x, mm->get_relative().y);
			drag_accum -= motion;

			if (beyond_deadzone || (scroll_h && Math::abs(drag_accum.x) > deadzone) || (scroll_v && Math::abs(drag_accum.y) > deadzone)) {
				if (!beyond_deadzone) {
					propagate_notification(NOTIFICATION_SCROLL_BEGIN);
					emit_signal("scroll_started");

					beyond_deadzone = true;

					drag_accum = -motion;
				}
				Vector2 diff = drag_from + drag_accum;
				if (scroll_h)
					h_scroll->set_value(diff.x);
				else
					drag_accum.x = 0;
				if (scroll_v)
					v_scroll->set_value(diff.y);
				else
					drag_accum.y = 0;
			}

			drag_speed -= motion;
		}
	}

	if (!drag_touching) {
		Ref<InputEventPanGesture> pan_gesture = p_event;
		if (pan_gesture.is_valid()) {

			if (h_scroll->is_visible_in_tree()) {
				h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);
			}
			if (v_scroll->is_visible_in_tree()) {
				v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);
			}
		}
	}

	if (!mm.is_valid() && !mb.is_valid()) {
		if (p_event->is_action_pressed("ui_page_up", true)) {
			if (v_scroll->is_visible_in_tree()) {
				_button_scroll(false, -1/scroll_step);
				accept_event();
			}
		}
		if (p_event->is_action_pressed("ui_page_down", true)) {
			if (v_scroll->is_visible_in_tree()) {
				_button_scroll(false, 1/scroll_step);
				accept_event();
			}
		}
	}

	if (v_scroll->get_value() != prev_v_scroll || h_scroll->get_value() != prev_h_scroll)
		accept_event(); //accept event if scroll changed
}

void ScrollContainer::_update_scrollbar_position() {

	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	h_scroll->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 0);
	h_scroll->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0);
	h_scroll->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -hmin.height);
	h_scroll->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0);

	v_scroll->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, -vmin.width);
	v_scroll->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0);
	v_scroll->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 0);
	v_scroll->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0);

	h_scroll->raise();
	v_scroll->raise();
}

void ScrollContainer::_ensure_focused_visible(Control *p_control) {
	if (!follow_focus) {
		return;
	}

	if (is_a_parent_of(p_control)) {
		Rect2 global_rect = get_global_rect();
		Rect2 other_rect = p_control->get_global_rect();
		float right_margin = 0;
		if (v_scroll->is_visible()) {
			right_margin += v_scroll->get_size().x;
		}
		float bottom_margin = 0;
		if (h_scroll->is_visible()) {
			bottom_margin += h_scroll->get_size().y;
		}

		float v_diff = MAX(MIN(other_rect.position.y, global_rect.position.y), other_rect.position.y + other_rect.size.y - global_rect.size.y + bottom_margin) - global_rect.position.y;
		float h_diff = MAX(MIN(other_rect.position.x, global_rect.position.x), other_rect.position.x + other_rect.size.x - global_rect.size.x + right_margin) - global_rect.position.x;

		if (always_smoothed) {
			if (h_diff != 0 || v_diff != 0) {
				_cancel_drag();
				inertial_target = Vector2(h_scroll->get_value() + h_diff, v_scroll->get_value() + v_diff);
				inertial_time_left = smooth_scroll_duration_button;
				_start_inertial_scroll();
				emit_signal("scroll_started");
				propagate_notification(NOTIFICATION_SCROLL_BEGIN);
			}
		} else {
			h_scroll->set_value(h_scroll->get_value() + h_diff);
			v_scroll->set_value(v_scroll->get_value() + v_diff);
		}
		
	}
}

void ScrollContainer::_notification(int p_what) {

	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {

		call_deferred("_update_scrollbar_position");
	};

	if (p_what == NOTIFICATION_READY) {

		get_viewport()->connect("gui_focus_changed", this, "_ensure_focused_visible");
	}

	if (p_what == NOTIFICATION_SORT_CHILDREN) {

		child_max_size = Size2(0, 0);
		Size2 size = get_size();
		Point2 ofs;

		Ref<StyleBox> sb = get_stylebox("bg");
		size -= sb->get_minimum_size();
		ofs += sb->get_offset();

		if (h_scroll->is_visible_in_tree() && h_scroll->get_parent() == this) //scrolls may have been moved out for reasons
			size.y -= h_scroll->get_minimum_size().y;

		if (v_scroll->is_visible_in_tree() && v_scroll->get_parent() == this) //scrolls may have been moved out for reasons
			size.x -= v_scroll->get_minimum_size().x;

		for (int i = 0; i < get_child_count(); i++) {

			Control *c = Object::cast_to<Control>(get_child(i));
			if (!c)
				continue;
			if (c->is_set_as_toplevel())
				continue;
			if (c == h_scroll || c == v_scroll)
				continue;
			Size2 minsize = c->get_combined_minimum_size();
			child_max_size.x = MAX(child_max_size.x, minsize.x);
			child_max_size.y = MAX(child_max_size.y, minsize.y);

			Rect2 r = Rect2(-scroll, minsize);
			if (!scroll_h || (!h_scroll->is_visible_in_tree() && c->get_h_size_flags() & SIZE_EXPAND)) {
				r.position.x = 0;
				if (c->get_h_size_flags() & SIZE_EXPAND)
					r.size.width = MAX(size.width, minsize.width);
				else
					r.size.width = minsize.width;
			}
			if (!scroll_v || (!v_scroll->is_visible_in_tree() && c->get_v_size_flags() & SIZE_EXPAND)) {
				r.position.y = 0;
				if (c->get_v_size_flags() & SIZE_EXPAND)
					r.size.height = MAX(size.height, minsize.height);
				else
					r.size.height = minsize.height;
			}
			r.position += ofs;
			fit_child_in_rect(c, r);
		}

		update();
	};

	if (p_what == NOTIFICATION_DRAW) {

		Ref<StyleBox> sb = get_stylebox("bg");
		draw_style_box(sb, Rect2(Vector2(), get_size()));

		update_scrollbars();
	}

	if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS) {
		if (animating) {
			inertial_time_left -= get_physics_process_delta_time();
			if (inertial_time_left <= 0) {
				inertial_time_left = 0;
			}

			float normalized_time = inertial_time_left/inertial_scroll_duration_current;
			Vector2 pos = Vector2().linear_interpolate(inertial_target, inertial_start, normalized_time*normalized_time);

			bool turnoff_h = !scroll_h;
			bool turnoff_v = !scroll_v;

			// // Stop and cap scroll value if reaching the end or beginning
			if (pos.x < 0) {
				pos.x = 0;
				turnoff_h = true;
			}
			if (pos.x > (h_scroll->get_max() - h_scroll->get_page())) {
				pos.x = h_scroll->get_max() - h_scroll->get_page();
				turnoff_h = true;
			}
			if (pos.y < 0) {
				pos.y = 0;
				turnoff_v = true;
			}
			if (pos.y > (v_scroll->get_max() - v_scroll->get_page())) {
				pos.y = v_scroll->get_max() - v_scroll->get_page();
				turnoff_v = true;
			}

			if (scroll_h) {
				h_scroll->set_value(pos.x);
			}
			if (scroll_v) {
				v_scroll->set_value(pos.y);
			}

			// If the animation is over, or if scrolling has stopped due to reaching the end or beginning, stop animating
			if ((turnoff_h && turnoff_v) || inertial_time_left <= 0) {
				_cancel_drag();
			}
		}
		if (drag_touching) {
			// Set the target to where the scroll will be if it continues for inertial_scroll_duration_touch seconds.
			Vector2 pos = Vector2(h_scroll->get_value(), v_scroll->get_value());
			inertial_target = pos + drag_speed * inertial_scroll_duration_touch * get_physics_process_delta_time() * 500; // 500 = 1000 ms in a second divided by 2 (slope of x^2 at x=1)
			// Reset drag_speed
			drag_speed = Vector2();
		}
	}
};

void ScrollContainer::update_scrollbars() {

	Size2 size = get_size();
	Ref<StyleBox> sb = get_stylebox("bg");
	size -= sb->get_minimum_size();

	Size2 hmin;
	Size2 vmin;
	if (scroll_h) {
		hmin = h_scroll->get_combined_minimum_size();
	}
	if (scroll_v) {
		vmin = v_scroll->get_combined_minimum_size();
	}

	Size2 min = child_max_size;

	bool hide_scroll_v = !scroll_v || min.height <= size.height;
	bool hide_scroll_h = !scroll_h || min.width <= size.width;

	v_scroll->set_max(min.height);
	if (hide_scroll_v) {
		v_scroll->set_page(size.height);
		v_scroll->hide();
		scroll.y = 0;
	} else {

		v_scroll->show();
		if (hide_scroll_h) {
			v_scroll->set_page(size.height);
		} else {
			v_scroll->set_page(size.height - hmin.height);
		}

		scroll.y = v_scroll->get_value();
	}

	h_scroll->set_max(min.width);
	if (hide_scroll_h) {
		h_scroll->set_page(size.width);
		h_scroll->hide();
		scroll.x = 0;
	} else {

		h_scroll->show();
		if (hide_scroll_v) {
			h_scroll->set_page(size.width);
		} else {
			h_scroll->set_page(size.width - vmin.width);
		}

		scroll.x = h_scroll->get_value();
	}

	// Avoid scrollbar overlapping.
	h_scroll->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, hide_scroll_v ? 0 : -vmin.width);
	v_scroll->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, hide_scroll_h ? 0 : -hmin.height);
}

void ScrollContainer::_scroll_moved(float) {

	scroll.x = h_scroll->get_value();
	scroll.y = v_scroll->get_value();
	queue_sort();

	update();
};

void ScrollContainer::set_enable_h_scroll(bool p_enable) {
	if (scroll_h == p_enable) {
		return;
	}

	scroll_h = p_enable;
	minimum_size_changed();
	queue_sort();
}

bool ScrollContainer::is_h_scroll_enabled() const {

	return scroll_h;
}

void ScrollContainer::set_enable_v_scroll(bool p_enable) {
	if (scroll_v == p_enable) {
		return;
	}

	scroll_v = p_enable;
	minimum_size_changed();
	queue_sort();
}

bool ScrollContainer::is_v_scroll_enabled() const {

	return scroll_v;
}

int ScrollContainer::get_v_scroll() const {

	return v_scroll->get_value();
}
void ScrollContainer::set_v_scroll(int p_pos) {

	v_scroll->set_value(p_pos);
	_cancel_drag();
}

int ScrollContainer::get_h_scroll() const {

	return h_scroll->get_value();
}
void ScrollContainer::set_h_scroll(int p_pos) {

	h_scroll->set_value(p_pos);
	_cancel_drag();
}

int ScrollContainer::get_deadzone() const {
	return deadzone;
}

void ScrollContainer::set_deadzone(int p_deadzone) {
	deadzone = p_deadzone;
}

bool ScrollContainer::is_following_focus() const {
	return follow_focus;
}

void ScrollContainer::set_follow_focus(bool p_follow) {
	follow_focus = p_follow;
}

bool ScrollContainer::is_always_smoothed() const {
	return always_smoothed;
}

void ScrollContainer::set_always_smoothed(bool p_enabled) {
	always_smoothed = p_enabled;
}

float ScrollContainer::get_scroll_step() const {
	return scroll_step;
}

void ScrollContainer::set_scroll_step(float p_value) {
	scroll_step = p_value;
}

String ScrollContainer::get_configuration_warning() const {

	String warning = Control::get_configuration_warning();

	int found = 0;

	for (int i = 0; i < get_child_count(); i++) {

		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c)
			continue;
		if (c->is_set_as_toplevel())
			continue;
		if (c == h_scroll || c == v_scroll)
			continue;

		found++;
	}

	if (found != 1) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("ScrollContainer is intended to work with a single child control.\nUse a container as child (VBox, HBox, etc.), or a Control and set the custom minimum size manually.");
	}

	return warning;
}

HScrollBar *ScrollContainer::get_h_scrollbar() {

	return h_scroll;
}

VScrollBar *ScrollContainer::get_v_scrollbar() {

	return v_scroll;
}

void ScrollContainer::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_scroll_moved"), &ScrollContainer::_scroll_moved);
	ClassDB::bind_method(D_METHOD("_gui_input"), &ScrollContainer::_gui_input);
	ClassDB::bind_method(D_METHOD("set_enable_h_scroll", "enable"), &ScrollContainer::set_enable_h_scroll);
	ClassDB::bind_method(D_METHOD("is_h_scroll_enabled"), &ScrollContainer::is_h_scroll_enabled);
	ClassDB::bind_method(D_METHOD("set_enable_v_scroll", "enable"), &ScrollContainer::set_enable_v_scroll);
	ClassDB::bind_method(D_METHOD("is_v_scroll_enabled"), &ScrollContainer::is_v_scroll_enabled);
	ClassDB::bind_method(D_METHOD("_update_scrollbar_position"), &ScrollContainer::_update_scrollbar_position);
	ClassDB::bind_method(D_METHOD("_ensure_focused_visible"), &ScrollContainer::_ensure_focused_visible);
	ClassDB::bind_method(D_METHOD("set_h_scroll", "value"), &ScrollContainer::set_h_scroll);
	ClassDB::bind_method(D_METHOD("get_h_scroll"), &ScrollContainer::get_h_scroll);
	ClassDB::bind_method(D_METHOD("set_v_scroll", "value"), &ScrollContainer::set_v_scroll);
	ClassDB::bind_method(D_METHOD("get_v_scroll"), &ScrollContainer::get_v_scroll);
	ClassDB::bind_method(D_METHOD("set_deadzone", "deadzone"), &ScrollContainer::set_deadzone);
	ClassDB::bind_method(D_METHOD("get_deadzone"), &ScrollContainer::get_deadzone);
	ClassDB::bind_method(D_METHOD("set_follow_focus", "enabled"), &ScrollContainer::set_follow_focus);
	ClassDB::bind_method(D_METHOD("is_following_focus"), &ScrollContainer::is_following_focus);
	ClassDB::bind_method(D_METHOD("set_always_smoothed", "enabled"), &ScrollContainer::set_always_smoothed);
	ClassDB::bind_method(D_METHOD("is_always_smoothed"), &ScrollContainer::is_always_smoothed);
	ClassDB::bind_method(D_METHOD("set_scroll_step", "value"), &ScrollContainer::set_scroll_step);
	ClassDB::bind_method(D_METHOD("get_scroll_step"), &ScrollContainer::get_scroll_step);

	ClassDB::bind_method(D_METHOD("get_h_scrollbar"), &ScrollContainer::get_h_scrollbar);
	ClassDB::bind_method(D_METHOD("get_v_scrollbar"), &ScrollContainer::get_v_scrollbar);

	ADD_SIGNAL(MethodInfo("scroll_started"));
	ADD_SIGNAL(MethodInfo("scroll_ended"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "follow_focus"), "set_follow_focus", "is_following_focus");

	ADD_GROUP("Scroll", "scroll_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_horizontal_enabled"), "set_enable_h_scroll", "is_h_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_horizontal"), "set_h_scroll", "get_h_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_vertical_enabled"), "set_enable_v_scroll", "is_v_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_vertical"), "set_v_scroll", "get_v_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scroll_deadzone"), "set_deadzone", "get_deadzone");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_smoothed"), "set_always_smoothed", "is_always_smoothed");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "scroll_step"), "set_scroll_step", "get_scroll_step");

	GLOBAL_DEF("gui/scroll/default_scroll_step", 0.125);
	GLOBAL_DEF("gui/scroll/default_scroll_deadzone", 0);
	GLOBAL_DEF("gui/scroll/default_scroll_smoothed", false);
	GLOBAL_DEF("gui/scroll/smooth_scroll_duration_button", 0.2);
	GLOBAL_DEF("gui/scroll/inertial_scroll_duration_touch", 1.5);
};

ScrollContainer::ScrollContainer() {

	h_scroll = memnew(HScrollBar);
	h_scroll->set_name("_h_scroll");
	add_child(h_scroll);
	h_scroll->connect("value_changed", this, "_scroll_moved");

	v_scroll = memnew(VScrollBar);
	v_scroll->set_name("_v_scroll");
	add_child(v_scroll);
	v_scroll->connect("value_changed", this, "_scroll_moved");

	drag_speed = Vector2();
	drag_touching = false;
	animating = false;
	beyond_deadzone = false;
	scroll_h = true;
	scroll_v = true;

	scroll_step = GLOBAL_GET("gui/scroll/default_scroll_step");
	deadzone = GLOBAL_GET("gui/scroll/default_scroll_deadzone");
	always_smoothed = GLOBAL_GET("gui/scroll/default_scroll_smoothed");
	inertial_scroll_duration_touch = GLOBAL_GET("gui/scroll/inertial_scroll_duration_touch");
	smooth_scroll_duration_button = GLOBAL_GET("gui/scroll/smooth_scroll_duration_button");

	follow_focus = false;

	set_clip_contents(true);
};
