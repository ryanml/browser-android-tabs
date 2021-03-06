// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/voice_interaction/voice_interaction_controller.h"

#include <memory>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
namespace {

class TestVoiceInteractionObserver : public mojom::VoiceInteractionObserver {
 public:
  TestVoiceInteractionObserver() : voice_interaction_binding_(this) {}

  ~TestVoiceInteractionObserver() override = default;

  // mojom::VoiceInteractionObserver overrides:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override {
    state_ = state;
  }
  void OnVoiceInteractionSettingsEnabled(bool enabled) override {
    settings_enabled_ = enabled;
  }
  void OnVoiceInteractionContextEnabled(bool enabled) override {
    context_enabled_ = enabled;
  }
  void OnVoiceInteractionHotwordEnabled(bool enabled) override {
    hotword_enabled_ = enabled;
  }
  void OnVoiceInteractionSetupCompleted(bool completed) override {
    setup_completed_ = completed;
  }
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override {}

  mojom::VoiceInteractionState voice_interaction_state() const {
    return state_;
  }
  bool settings_enabled() const { return settings_enabled_; }
  bool context_enabled() const { return context_enabled_; }
  bool hotword_enabled() const { return hotword_enabled_; }
  bool setup_completed() const { return setup_completed_; }

  void SetVoiceInteractionController(VoiceInteractionController* controller) {
    mojom::VoiceInteractionObserverPtr ptr;
    voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
    controller->AddObserver(std::move(ptr));
  }

 private:
  mojom::VoiceInteractionState state_ = mojom::VoiceInteractionState::STOPPED;
  bool settings_enabled_ = false;
  bool context_enabled_ = false;
  bool hotword_enabled_ = false;
  bool setup_completed_ = false;

  mojo::Binding<mojom::VoiceInteractionObserver> voice_interaction_binding_;

  DISALLOW_COPY_AND_ASSIGN(TestVoiceInteractionObserver);
};

class VoiceInteractionControllerTest : public AshTestBase {
 public:
  VoiceInteractionControllerTest() = default;
  ~VoiceInteractionControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    observer_ = std::make_unique<TestVoiceInteractionObserver>();
    observer_->SetVoiceInteractionController(controller());
  }

  void TearDown() override {
    observer_.reset();

    AshTestBase::TearDown();
  }

 protected:
  VoiceInteractionController* controller() {
    return Shell::Get()->voice_interaction_controller();
  }

  TestVoiceInteractionObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<TestVoiceInteractionObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionControllerTest);
};

}  // namespace

TEST_F(VoiceInteractionControllerTest, NotifyStatusChanged) {
  controller()->NotifyStatusChanged(mojom::VoiceInteractionState::RUNNING);
  controller()->FlushForTesting();

  // The cached state should be updated.
  EXPECT_EQ(mojom::VoiceInteractionState::RUNNING,
            controller()->voice_interaction_state());
  // The observers should be notified.
  EXPECT_EQ(mojom::VoiceInteractionState::RUNNING,
            observer()->voice_interaction_state());
}

TEST_F(VoiceInteractionControllerTest, NotifySettingsEnabled) {
  controller()->NotifySettingsEnabled(true);
  controller()->FlushForTesting();
  // The cached state should be updated.
  EXPECT_TRUE(controller()->settings_enabled());
  // The observers should be notified.
  EXPECT_TRUE(observer()->settings_enabled());
}

TEST_F(VoiceInteractionControllerTest, NotifyContextEnabled) {
  controller()->NotifyContextEnabled(true);
  controller()->FlushForTesting();
  // The observers should be notified.
  EXPECT_TRUE(observer()->context_enabled());
}

TEST_F(VoiceInteractionControllerTest, NotifyHotwordEnabled) {
  controller()->NotifyHotwordEnabled(true);
  controller()->FlushForTesting();
  // The observers should be notified.
  EXPECT_TRUE(observer()->hotword_enabled());
}

TEST_F(VoiceInteractionControllerTest, NotifySetupCompleted) {
  controller()->NotifySetupCompleted(true);
  controller()->FlushForTesting();
  // The cached state should be updated.
  EXPECT_TRUE(controller()->setup_completed());
  // The observers should be notified.
  EXPECT_TRUE(observer()->setup_completed());
}

}  // namespace ash
